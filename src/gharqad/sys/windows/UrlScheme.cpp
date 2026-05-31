#include <nekobox/sys/UrlScheme.hpp>
#include <nekobox/sys/Settings.h>

#include <QApplication>
#include <QDir>
#include <QSettings>

static const char * kSchemeRoot = "HKEY_CURRENT_USER\\Software\\Classes\\nekoray";

QString UrlScheme_DesiredState() {
    const QString exe = getApplicationPath();
    return "\"" + exe + "\" \"-deeplink\" \"%1\"";
}

void UrlScheme_Apply() {
    QSettings reg(kSchemeRoot, QSettings::NativeFormat);
    reg.setValue("Default", "URL:NekoBox Protocol");
    reg.setValue("URL Protocol", "");
    reg.setValue("shell/open/command/Default", UrlScheme_DesiredState());
}
