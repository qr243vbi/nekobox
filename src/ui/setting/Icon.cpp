#include "include/ui/setting/Icon.hpp"

#include "include/global/Configs.hpp"
#include <QCoreApplication>
#include <QPainter>
#include <QDir>

#ifndef SYSTRAY_ICON_DIR
#ifdef Q_OS_MACOS
#define SYSTRAY_ICON_DIR ":/systray/"
#define SYSTRAY_ICON(X) QString(SYSTRAY_ICON_DIR) + X
#else
#include "include/sys/Settings.h"
#define SYSTRAY_ICON(X) getResource(X)
#endif
#endif

#define FORMAT nullptr

QPixmap Icon::GetTrayIcon(TrayIconStatus status) {
    QPixmap pixmap;

    if (status == NONE)
    {
        auto pixmap_read = QPixmap(SYSTRAY_ICON("Off.png"), FORMAT);
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == RUNNING)
    {
        auto pixmap_read = QPixmap(SYSTRAY_ICON("On.png"), FORMAT);
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == SYSTEM_PROXY_DNS)
    {
        auto pixmap_read = QPixmap(SYSTRAY_ICON("Proxy-Dns.png"), FORMAT);
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == SYSTEM_PROXY)
    {
        auto pixmap_read = QPixmap(SYSTRAY_ICON("Proxy.png"), FORMAT);
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == DNS)
    {
        auto pixmap_read = QPixmap(SYSTRAY_ICON("Dns.png"), FORMAT);
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == VPN)
    {
        auto pixmap_read = QPixmap(SYSTRAY_ICON("Tun.png"), FORMAT);
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else
    {
        MW_show_log("Icon::GetTrayIcon: Unknown status");
        auto pixmap_read = QPixmap(SYSTRAY_ICON("Off.png"), FORMAT);
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    }

    return pixmap;
}
