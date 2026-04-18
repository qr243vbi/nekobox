#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include <memory>
#include <QElapsedTimer>
#include <QProcess>
#include <QMutex>
#include <functional>

#undef ELEVATE_METHOD
#ifdef Q_OS_UNIX
#define ELEVATE_METHOD
#endif
#ifdef Q_OS_WIN
#define ELEVATE_METHOD
#endif

namespace Configs_sys {

    class CoreProcess: public QObject
    {
    public:
        bool save_elevated = false;
        QProcess process;
        QString tag;
        QString program;
        QStringList arguments;
        int waitpid;
        std::string * domain;
        int * port;
        std::function<void()> core_pre_start;

        ~CoreProcess();

        // start & kill is one time

        void Start();

        void Kill();

        void Restart();

#ifdef ELEVATE_METHOD
        void elevateCoreProcessProgram();
#endif

        CoreProcess(const QString &core_path, const QStringList &args, std::string *, int *, std::function<void()> );

        int start_profile_when_core_is_up = -1;

    private:
        bool show_stderr = false;
        bool failed_to_start = false;
        QMutex restarting;

        QElapsedTimer coreRestartTimer;

    protected:
        bool started = false;
        bool crashed = false;

#ifdef ELEVATE_METHOD
        bool coreProcessProgramElevated = false;
#endif
    };

    inline QAtomicInt logCounter;
} // namespace Configs_sys
