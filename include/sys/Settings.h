#ifndef NEKOBOX_SETTINGS
#define NEKOBOX_SETTINGS
#include <QSettings>

#define CONFIG_INI_PATH  QDir::current().absolutePath() + "/window.ini"

bool createSymlink(const QString &targetPath, const QString &linkPath);
QSettings getSettings();
QString getResourcesDir();
QString getResource(QString);
#ifndef SKIP_UPDATER_BUTTON
QString getUpdaterPath();
#endif
#ifdef Q_OS_LINUX
bool isAppImage();
#endif
QString getRootResource(QString str);
QString getCorePath();
QString getApplicationPath();
bool isFileAppendable(QString filePath);
bool isDirectoryWritable(QString filePath);

#endif
