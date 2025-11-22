#include "include/ui/setting/Icon.hpp"

#include "include/global/Configs.hpp"
#include <QCoreApplication>
#include <QPainter>
#include <QDir>

#ifndef SYSTRAY_ICON_DIR
#include "include/sys/Settings.h"
#define SYSTRAY_ICON(X) getResource(X)
#endif

#define FORMAT nullptr

QPixmap Icon::GetTrayIcon(TrayIconStatus status) {
    QPixmap pixmap;
    auto pixmap_read = QPixmap(SYSTRAY_ICON("On.png"), FORMAT);
    if (!pixmap_read.isNull()) pixmap = pixmap_read;

    if (status == TrayIconStatus::NONE) return pixmap;

    auto p = QPainter(&pixmap);
    auto side = pixmap.width();
    auto radius = side * 0.4;
    auto d = side * 0.4;
    auto margin = side * 0.04;

    if (status == TrayIconStatus::RUNNING) {
        p.setBrush(QBrush(Qt::darkGreen));
    } else if (status == TrayIconStatus::SYSTEM_PROXY) {
        p.setBrush(QBrush(Qt::blue));
    } else if (status == TrayIconStatus::VPN) {
        p.setBrush(QBrush(Qt::red));
    } else if (status == TrayIconStatus::DNS) {
        p.setBrush(QBrush(Qt::darkMagenta));
    }
    p.drawRoundedRect(
        QRect(side - d - margin,
              side - d - margin,
              d,
              d),
              radius,
              radius);
    p.end();

    return pixmap;
}
