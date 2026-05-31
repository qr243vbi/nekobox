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

    std::map<std::basic_string<char>, std::basic_string<char>> desktop;
    auto desired = UrlScheme_DesiredState().toStdString();

    {
        std::ifstream ifdesktop(path.toStdString());
        if (ifdesktop.is_open()) {
            inipp::Ini<char> ini;
            ini.parse(ifdesktop);
            desktop = ini.sections["Desktop Entry"];

            auto iter = desktop.find("Exec");
            if (iter != desktop.end() && iter->second == desired){
                if (ini.sections.size() == 1){
                    return;
                }
            }
        }
    }
 
    desktop["Type"] = "Application";
    desktop["Name"] = software_name.toStdString();
    desktop["Exec"] = desired;
    desktop["MimeType"] = "x-scheme-handler/nekoray;";
    desktop["Terminal"] = "false";
    desktop["NoDisplay"] = "true";

    {
        std::ofstream ofdesktop(path.toStdString());
        if (ofdesktop.is_open()) {
            inipp::Ini<char> ini;
            ini.sections["Desktop Entry"] = desktop;
            ini.strip_trailing_comments();
            ini.generate(ofdesktop);
        } else {
            return;
        }
    }

    // 7. Fix: Convert update-desktop-database paths to a native, flat text format
    QProcess::execute("update-desktop-database", {QDir::toNativeSeparators(dir)});
    QProcess::execute("xdg-mime", {"default", kDesktopId, "x-scheme-handler/nekoray"});
}