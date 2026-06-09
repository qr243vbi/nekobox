#include <nekobox/sys/UrlScheme.hpp>
#include <nekobox/sys/Settings.h>

#include <QApplication>
#include <QDir>
#include <QSettings>

static const char * kSchemeRoot = "HKEY_CURRENT_USER\\Software\\Classes\\nekoray";

static QString UrlScheme_DesiredState() {
    const QString exe = getApplicationPath();
    return "\"" + exe + "\" \"-deeplink\" \"%1\"";
}

void UrlScheme_RegisterIfNeeded() {
    const QString desired = UrlScheme_DesiredState();
    QSettings reg(kSchemeRoot, QSettings::NativeFormat);
    if (reg.value("shell/open/command/Default", "").toString() == desired){
        return;
    }
    reg.setValue("Default", "URL:NekoBox Protocol");
    reg.setValue("URL Protocol", "");
    reg.setValue("shell/open/command/Default", desired);
    reg.sync();
}
