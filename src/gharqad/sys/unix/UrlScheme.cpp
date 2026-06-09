#include <nekobox/sys/UrlScheme.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/sys/Settings.h>
#include <3rdparty/ini/ini.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>
#include <QProcess>
#include <fstream>

static const QString kDesktopId = "x-scheme-handler-iblis.desktop";

static QString UrlScheme_DesiredState() {
    return joinCommand({getApplicationPath(),"-deeplink", "%u"});
}

void UrlScheme_RegisterIfNeeded() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (dir.isEmpty()) {
        return; 
    }

    QDir().mkpath(dir);

    const QString path = QDir(dir).filePath(kDesktopId);
    auto desired = UrlScheme_DesiredState();

    QMap<QString, QString> desktop;

    desktop["Type"] = "Application";
    desktop["Name"] = software_name;
    desktop["Exec"] = desired;
    desktop["MimeType"] = "x-scheme-handler/nekoray;";
    desktop["Terminal"] = "false";
    desktop["NoDisplay"] = "true";

    QFile desktopFile(path);

    {
        if (desktopFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            iniqt::Ini ini;
            QTextStream inStream(&desktopFile);
            ini.parse(inStream);
            desktopFile.close();
            
            auto seciter = ini.sections.find("Desktop Entry");
            if (seciter != ini.sections.end()){
                auto desktop2 = seciter.value();
                if (desktop2 == desktop && ini.sections.count() == 1){
                    return;
                }
            }
        }
    }
 
    {
        if (desktopFile.open(QIODevice::WriteOnly | QIODevice::Text))  {
            iniqt::Ini ini;
            QTextStream outStream(&desktopFile);
            ini.sections["Desktop Entry"] = desktop;
            ini.generate(outStream);
            desktopFile.close();
            QProcess::execute("update-desktop-database", {QDir::toNativeSeparators(dir)});
            QProcess::execute("xdg-mime", {"default", kDesktopId, "x-scheme-handler/nekoray"});
        }
    }

    
}