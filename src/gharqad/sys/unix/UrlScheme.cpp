#include <nekobox/sys/UrlScheme.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/sys/Settings.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>
#include <QProcess>

static const QString kDesktopId = "nekobox_url_handler.desktop";

QString UrlScheme_DesiredState() {
    return joinCommand({getApplicationPath(),"-deeplink", "%u"});
}

void UrlScheme_Apply() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    const QString path = dir + "/" + kDesktopId;

    WriteFileText( path, 
        QString("[Desktop Entry]\n")     +
        "Type=Application\n"    +
        "Name=NyameBox\n"       +
        "Exec=" + UrlScheme_DesiredState() + "\n" +
        "MimeType=x-scheme-handler/nekoray;\n" +
        "Terminal=false\n" +
        "NoDisplay=true\n"
    );
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QProcess::execute("update-desktop-database", {appsDir});
    QProcess::execute("xdg-mime", {"default", kDesktopId, "x-scheme-handler/nekoray"});
}
