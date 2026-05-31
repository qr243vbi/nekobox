#include <nekobox/sys/UrlScheme.hpp>

#include <nekobox/sys/Settings.h>

void UrlScheme_RegisterIfNeeded() {
    const QString desired = UrlScheme_DesiredState();
    if (desired.isEmpty()) return;

    if (Configs::windowSettings->url_scheme_mirror == desired) return;

    UrlScheme_Apply();
    Configs::windowSettings->url_scheme_mirror = desired;
    Configs::windowSettings->Save();
}
