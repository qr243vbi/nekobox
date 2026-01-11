#include "nekobox/ui/setting/Icon.hpp"
#include "nekobox/global/GuiUtils.hpp"
#include <QCoreApplication>
#include <QPainter>
#include <QDir>
#include <qicon.h>

#ifndef SYSTRAY_ICON_DIR
#include "nekobox/sys/Settings.h"
#define SYSTRAY_ICON(X) getResource(X)
#endif

#define FORMAT nullptr

QPixmap Icon::GetTrayIcon(TrayIconStatus status) {
    QPixmap pixmap(256, 256);

    auto pixmap_read = QPixmap(SYSTRAY_ICON("icon.png"), FORMAT);
    bool pixmap_read_isnull = pixmap_read.isNull();
    if (!pixmap_read_isnull){
        pixmap = pixmap_read;
    }
    if (!indicatorRuleMap.contains(status)) {
        return pixmap;
    }
    auto rule = indicatorRuleMap[status];
    auto p = QPainter(&pixmap);
    if (pixmap_read_isnull){
        QIcon icon = QIcon::fromTheme("nekobox");
        icon.paint(&p, QRect(0, 0, 256, 256));
    }

    auto side = pixmap.width();
    auto radius = side * rule.radius;
    auto d = side * rule.diameter;
    auto margin = side * rule.margin;
    
    p.setBrush(QBrush(rule.color));
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
