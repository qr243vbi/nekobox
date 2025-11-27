#include "include/sys/linux/LinuxCap.h"

#include <QDebug>
#include <QProcess>
#include <QStandardPaths>

bool Linux_HavePkexec() {
    QProcess p;
    p.setProgram("pkexec");
    p.setArguments({"--help"});
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start();
    p.waitForFinished(500);
    return (p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1) == 0;
}
