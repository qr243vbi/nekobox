#include <QFile>
#include <QFileInfo>
#include <QString>
#include <main/NekoGui_Utils.hpp>

int Mac_Run_Command(QString command) {
    auto cmd = QString("osascript -e 'tell application \"Terminal\" to activate' -e 'tell application \"Terminal\" to do script \"%1; exit;\"' with administrator privileges").arg(command);
    return system(cmd.toStdString().c_str());
}
