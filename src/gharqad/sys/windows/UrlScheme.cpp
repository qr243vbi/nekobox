#include <nekobox/sys/UrlScheme.hpp>
#include <nekobox/sys/Settings.h>

#include <QApplication>
#include <QDir>
#include <QSettings>

static const char * kSchemes[] = {
    "nekoray",
    "nekobox",
    "v2raytun",
};

static QString UrlScheme_DesiredState() {
    const QString exe = getApplicationPath();
    return "\"" + exe + "\" \"-deeplink\" \"%1\"";
}

void UrlScheme_RegisterIfNeeded() {
    const QString desired = UrlScheme_DesiredState();
    for (const auto *scheme : kSchemes) {
        QSettings reg("HKEY_CURRENT_USER\\Software\\Classes\\" + QString::fromLatin1(scheme),
                      QSettings::NativeFormat);
        if (reg.value("shell/open/command/Default", "").toString() == desired){
            continue;
        }
        reg.setValue("Default", "URL:NekoBox Protocol");
        reg.setValue("URL Protocol", "");
        reg.setValue("shell/open/command/Default", desired);
        reg.sync();
    }
}
