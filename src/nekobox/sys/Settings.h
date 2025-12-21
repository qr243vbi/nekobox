#ifndef NEKOBOX_SETTINGS
#define NEKOBOX_SETTINGS
#include <QSettings>
#include <QList>

#define CONFIG_INI_PATH  QDir::current().absolutePath() + "/window.ini"

void updateEmojiFont();

bool createSymlink(const QString &targetPath, const QString &linkPath);
QSettings getSettings();
QString getLocale();
QString getResourcesDir();
QString getResource(QString);
#ifndef SKIP_UPDATER_BUTTON
QString getUpdaterPath();
#endif
#ifdef Q_OS_UNIX
bool isAppImage();
#endif
QString getRootResource(QString str);
QString getCorePath();
QString getApplicationPath();
bool isFileAppendable(QString filePath);
bool isDirectoryWritable(QString filePath);

struct MainWindowTableSettings{
    bool manually_column_width = false;
    int column_width[5];
    void Save(QSettings & settings);
    void Load(QSettings & settings);
};
namespace Configs{
    extern MainWindowTableSettings tableSettings;
}

#endif
