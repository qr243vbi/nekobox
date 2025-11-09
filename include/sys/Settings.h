#ifndef NEKOBOX_SETTINGS
#define NEKOBOX_SETTINGS
#include <QSettings>
#include <QDir>
#include <QCoreApplication>

#define CONFIG_INI_PATH  QDir::current().absolutePath() + "/window.ini"
QSettings getSettings();
QString getResourcesDir();
QString getUpdaterPath();
QString getResource(QString);
QString getUpdater();
QString getRootResource(QString str);
QString getCorePath();

#endif
