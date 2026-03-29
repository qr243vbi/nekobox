#include <nekobox/ui/setting/Icon.hpp>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/ui/info/info.h>
#include <QCoreApplication>
#include <QDir>
#include <QPainter>
#include <nekobox/stats/traffic/TrafficLooper.hpp>
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
  }
  p.end();
  return pixmap;
}

#define SET_TRAFFIC_STAT(type, way) this->ui->total_##type##_##way##load->setText(QString::number(Stats::trafficLooper->total_##type##_##way##load()));

InfoDialog::InfoDialog(QWidget *parent) : QDialog(parent), ui(new Ui::InfoMain)  {
  ui->setupUi(this);
  Stats::trafficLooper->initialize();
  SET_TRAFFIC_STAT(direct, down)
  SET_TRAFFIC_STAT(direct, up)
  SET_TRAFFIC_STAT(proxy, down)
  SET_TRAFFIC_STAT(proxy, up)
  
//  this->ui->total_direct_download->setText(Stats::trafficLooper->);
//  this->ui->total_direct_upload->setText();
//  this->ui->total_proxy_download->setText();
//  this->ui->total_proxy_upload->setText();
}

InfoDialog::~InfoDialog(){

}

void InfoDialog::accept(){

}