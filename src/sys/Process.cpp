#include "nekobox/sys/Process.hpp"
#include "nekobox/dataStore/Configs.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>
#include <QCoreApplication>
#include "nekobox/ui/mainwindow.h"

#undef ELEVATE_METHOD
#ifdef Q_OS_UNIX
#define ELEVATE_METHOD
#endif
#ifdef Q_OS_WIN
#define ELEVATE_METHOD
#endif

namespace Configs_sys {

    CoreProcess::~CoreProcess() {
    }

    void CoreProcess::Kill() {
        process.kill();
        process.waitForFinished();
    }

    CoreProcess::CoreProcess(const QString &core_path, const QStringList &args) {
        program = core_path;
        arguments = args;
        arguments << "-waitpid";
        arguments << QString::number(QCoreApplication::applicationPid());

        connect(&process, &QProcess::readyReadStandardOutput, this, [&]() {
            auto log = process.readAllStandardOutput();
            if (start_profile_when_core_is_up >= 0) {
                if (log.contains("Core listening at")) {
                    // The core really started
                    MW_dialog_message("ExternalProcess", "CoreStarted," + QString::number(start_profile_when_core_is_up));
                    start_profile_when_core_is_up = -1;
                } else if (log.contains("failed to serve")) {
                    // The core failed to start
                    process.kill();
                }
            }
            if (log.contains("Extra process exited unexpectedly"))
            {
                MW_show_log("Extra Core exited, stopping profile...");
                MW_dialog_message("ExternalProcess", "Crashed");
            }
            if (logCounter.fetchAndAddRelaxed(log.count("\n")) > Configs::dataStore->max_log_line) return;
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
                qDebug() << "Core stated changed to not running";
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
        if (started) return;
        started = true;
        QStringList list = QProcessEnvironment::systemEnvironment().toStringList();
        list << "NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE=nekobox_core";
        process.setEnvironment(list);
        process.start(program, arguments);
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
            coreProcessProgramElevated = true;
        }
    }
#undef ELEVATE_METHOD
#endif
} // namespace Configs_sys
