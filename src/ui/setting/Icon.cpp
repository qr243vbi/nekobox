#include "nekobox/ui/setting/Icon.hpp"
#include "nekobox/global/GuiUtils.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QPainter>
#include <qicon.h>

#ifndef SYSTRAY_ICON_DIR
#include "nekobox/sys/Settings.h"
#define SYSTRAY_ICON(X) getResource(X)
#endif

QPixmap Icon::GetTrayIcon(TrayIconStatus status) {
  QPixmap pixmap(256, 256);
  pixmap.fill(Qt::transparent);

  QIcon pixmap_read(SYSTRAY_ICON("icon.png"));
  bool pixmap_read_isnull = pixmap_read.isNull();
  auto p = QPainter(&pixmap);
  auto rule = indicatorRuleMap[status];
  if (pixmap_read_isnull) {
    pixmap_read = QIcon::fromTheme("nekobox");
    pixmap_read_isnull = pixmap_read.isNull();
  } 
  if (!pixmap_read_isnull){
    p.drawPixmap(0, 0, pixmap_read.pixmap(QSize(256, 256)));
<<<<<<< HEAD
  }
  if (indicatorRuleMap.contains(status)) {
    auto side = pixmap.width();
    auto radius = side * rule.radius;
    auto d = side * rule.diameter;
    auto margin = side * rule.margin;

    p.setBrush(QBrush(QListInt2Color(rule.color)));
    p.drawRoundedRect(QRect(side - d - margin, 
        side - d - margin, d, d), radius,
                      radius);
=======
    if (indicatorRuleMap.contains(status)) {
      auto side = pixmap.width();
      auto radius = side * rule.radius;
      auto d = side * rule.diameter;
      auto margin = side * rule.margin;

#ifdef DEBUG_MODE
      qDebug() << "ICON side" << side << "radius" << radius << "diameter" << d << "margin" << margin;
#endif
      if (radius > 0) {
        p.setBrush(QBrush(QListInt2Color(rule.color)));
        p.drawRoundedRect(QRect(side - d - margin,
           side - d - margin, d, d), radius, radius);
      }
    }
>>>>>>> other-repo/main
  }
  p.end();
  return pixmap;
}
