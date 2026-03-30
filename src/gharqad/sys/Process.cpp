#include "nekobox/sys/Process.hpp"
#include "nekobox/dataStore/Configs.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>
#include <QStandardPaths>
#include <QCoreApplication>
#include "nekobox/sys/Settings.h"
#include "nekobox/ui/mainwindow.h"

#undef ELEVATE_METHOD
#ifdef Q_OS_UNIX
#define ELEVATE_METHOD
#endif
#ifdef Q_OS_WIN
#define ELEVATE_METHOD
#endif

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <unistd.h>
#include <QFile>

static bool removeIfDifferentOwner(const QString &path)
{
    struct stat st;
    if (stat(path.toLocal8Bit().constData(), &st) != 0){
        return false; // file doesn't exist or error
    }

    uid_t processUid = getuid();   // or geteuid() if needed

    if (st.st_uid != processUid) {
        return QFile::remove(path);
    }

    return false; // same owner → keep file
}
#endif

namespace Configs_sys {
    CoreProcess::~CoreProcess() {
    }

    void CoreProcess::Kill() {
        process.kill();
        process.waitForFinished();
    }

    CoreProcess::CoreProcess(const QString &core_path, const QStringList &args, std::string * domain, int * port, std::function<void()> func) {
        program = core_path;
        arguments = args;
        this->core_pre_start = func;
        this->port = port;
        this->domain = domain;

        #ifdef DEBUG_MODE
        qDebug() << "CORE START WITH PATH" << program << "AND ARGS" << arguments ;
        #endif

        connect(&process, &QProcess::readyReadStandardOutput, this, [&]() {
            auto log = process.readAllStandardOutput();
            if (start_profile_when_core_is_up >= 0) {
                if (log.contains("Core listening")) {
                    // The core really started
                    MW_dialog_message("ExternalProcess", "CoreStarted," + QString::number(start_profile_when_core_is_up));
                    start_profile_when_core_is_up = -1;
                    goto show_log;
                } else if (log.contains("failed to serve")) {
                    // The core failed to start
                    process.kill();
                    goto show_log;
                }
            } else {
                if (log.contains("Core listening")) {
                    MW_dialog_message("ExternalProcess", "CoreStarted,-1");
                    goto show_log;
                }
            }
            if (log.contains("Extra process exited unexpectedly"))
            {
                MW_show_log("Extra Core exited, stopping profile...");
                MW_dialog_message("ExternalProcess", "Crashed");
                goto show_log;
            }
            show_log:
            if (logCounter.fetchAndAddRelaxed(log.count("\n")) > Configs::windowSettings->max_log_line) return;
            MW_show_log(log);
        });
        connect(&process, &QProcess::readyReadStandardError, this, [&]() {
            auto log = process.readAllStandardError().trimmed();
            MW_show_log(log);
        });
        connect(&process, &QProcess::errorOccurred, this, [&](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                failed_to_start = true;
                MW_show_log("start core error occurred: " + process.errorString() + "\n");
            }
        });
        connect(&process, &QProcess::stateChanged, this, [&](QProcess::ProcessState state) {
            if (state == QProcess::Running){
                Configs::dataStore->core_running = true;
            }

            if (state == QProcess::NotRunning) {
                Configs::dataStore->core_running = false;
                qWarning() << "Core stated changed to not running";
            }

            if (!Configs::dataStore->prepare_exit && state == QProcess::NotRunning) {
                if (failed_to_start) return; // no retry
                bool restarting = !this->restarting.tryLock();

                if (restarting) return;

                MW_show_log("[Fatal] " + QObject::tr("Core exited, cleaning up..."));
                GetMainWindow()->profile_stop(true, true);

                // Retry rate limit
                if (coreRestartTimer.isValid()) {
                    if (coreRestartTimer.restart() < 10 * 1000) {
                        coreRestartTimer = QElapsedTimer();
                        MW_show_log("[ERROR] " + QObject::tr("Core exits too frequently, stop automatic restart this profile."));
                        return;
                    }
                } else {
                    coreRestartTimer.start();
                }

                // Restart
                start_profile_when_core_is_up = Configs::dataStore->started_id;
                MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
                this->restarting.unlock();
                setTimeout([=,this] { Restart(); }, this, 200);
            }
        });
    }

    void CoreProcess::Start() {
        if (core_pre_start != nullptr){
            core_pre_start();
        }
        if (started) return;
        started = true;
        QStringList list = QProcessEnvironment::systemEnvironment().toStringList();

        QString rulesets = QDir("rule_sets").absolutePath();
        list << "NEKOBOX_RULESET_CACHE_DIRECTORY=" + rulesets;
        #ifdef DEBUG_MODE
        qDebug() << "RULESETS ARE IN " << rulesets;
        #endif

        list << "NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE=nekobox_core";


        auto cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(cachePath);//create parent dir tree

#ifdef Q_OS_UNIX
        QString cache_id_test = serverName + "-test.db";
        QString cache_id_core = serverName + "-core.db";
        removeIfDifferentOwner(cache_id_core);
        removeIfDifferentOwner(cache_id_test);
#endif

        process.setWorkingDirectory(cachePath);

        process.setEnvironment(list);
        QStringList args = arguments;

        args << "-waitpid";
        args << QString::number(QCoreApplication::applicationPid());
        args << "-address";
        args << QString::fromStdString(*this->domain);
        args << "-port";
        args << QString::number(*this->port);
        #ifdef DEBUG_MODE
        qDebug() <<" ARGUMENTS ARE " << args;
        #endif    
        process.setArguments(args);
        process.setProgram(program);
        process.start();
    }

    void CoreProcess::Restart() {
        if (!restarting.tryLock()) return;
        process.kill();
        process.waitForFinished(500);
        started = false;
        Start();
        restarting.unlock();
    }

#ifdef ELEVATE_METHOD
    void CoreProcess::elevateCoreProcessProgram(){
        if (!coreProcessProgramElevated){
            arguments.prepend("-admin");
#ifdef Q_OS_UNIX
            if (this->save_elevated){
                arguments.prepend("-save");
            }
#endif
            coreProcessProgramElevated = true;
        }
    }
#undef ELEVATE_METHOD
#endif
} // namespace Configs_sys
