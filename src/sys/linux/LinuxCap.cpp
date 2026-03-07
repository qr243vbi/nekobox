#include <nekobox/sys/linux/LinuxCap.h>
#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <sys/resource.h>

void Unix_SetCrashHandler() {
    rlimit rl;
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rl);
}

bool Unix_HavePkexec() {
    QProcess p;
    p.setProgram("pkexec");
    p.setArguments({"--help"});
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start();
    p.waitForFinished(500);
    return (p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1) == 0;
}
