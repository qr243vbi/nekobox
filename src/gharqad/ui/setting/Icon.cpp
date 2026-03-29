#include <nekobox/ui/setting/Icon.hpp>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/ui/info/info.h>
#ifdef NKR_SOFTWARE_KEYS
#include <nekobox/ui/mainwindow.h>
#include <nekobox/ui/security_addon.h>
#else
#define CHECK_SETTINGS_ACCESS
#endif
#include <QCoreApplication>
#include <QDir>
#include <QPainter>
#include <nekobox/stats/traffic/TrafficLooper.hpp>
#include <nekobox/dataStore/Database.hpp>
#include <qicon.h>

#ifndef SYSTRAY_ICON_DIR
#include <nekobox/sys/Settings.h>
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

#define SET_CUSTOM_STAT(name, value, FUNC) this->ui->name->setText(FUNC(value));
#define SET_LOGGER_STAT(name, FUNC) SET_CUSTOM_STAT(name, Stats::databaseLogger->name, FUNC);
#define SET_LOGGER_FUNC(name, FUNC) SET_CUSTOM_STAT(name, Stats::databaseLogger->get_##name(), FUNC);
#define SET_TRAFFIC_STAT(type, way, FUNC) SET_CUSTOM_STAT(total_##type##_##way##load, \
  Stats::trafficLooper->total_##type##_##way##load(), FUNC);
#define SET_DATA_STAT(proxy, profiles, WORD, FUNC) \
    SET_CUSTOM_STAT(proxy##_created, Stats::databaseLogger->profiles->created, FUNC); \
    SET_CUSTOM_STAT(proxy##_deleted, Stats::databaseLogger->profiles->deleted, FUNC); \
    SET_CUSTOM_STAT(proxy##_exists, WORD, FUNC); 

InfoDialog::InfoDialog(QWidget *parent) : QDialog(parent), ui(new Ui::InfoMain)  {
  CHECK_SETTINGS_ACCESS
  ui->setupUi(this);
  ui->textBrowser->document()->setDefaultFont(qApp->font());
  this->setWindowTitle(software_name);
  Stats::trafficLooper->initialize();
  SET_TRAFFIC_STAT(direct, down, ReadableSize)
  SET_TRAFFIC_STAT(direct, up, ReadableSize)
  SET_TRAFFIC_STAT(proxy, down, ReadableSize)
  SET_TRAFFIC_STAT(proxy, up, ReadableSize)
  SET_LOGGER_STAT(start_count, QString::number)
  SET_DATA_STAT(proxy, profiles, Configs::profileManager->profiles.size(), QString::number)
  SET_DATA_STAT(group, groups, Configs::profileManager->groups.size(), QString::number)
  SET_DATA_STAT(route, routes, Configs::profileManager->routes.size(), QString::number)
  SET_LOGGER_FUNC(usage_time, QString::number)
  SET_LOGGER_STAT(last_launch_time, QString::number)
  SET_LOGGER_STAT(first_launch_time, QString::number)
  
#ifdef NKR_SOFTWARE_KEYS
  SET_CUSTOM_STAT(users_count, getUserCount(), QString::number)
  SET_LOGGER_FUNC(failed_auth_count, QString::number)
#else 
  this->ui->security_group->hide();
  #endif
  
//  this->ui->total_direct_download->setText(Stats::trafficLooper->);
//  this->ui->total_direct_upload->setText();
//  this->ui->total_proxy_download->setText();
//  this->ui->total_proxy_upload->setText();
}

InfoDialog::~InfoDialog(){

}

void InfoDialog::accept(){

}