#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <nekobox/dataStore/Configs.hpp>

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static QMap<QString, QString> & getThemes();
    QString current_theme = "";
    void ApplyTheme(const QString &theme, bool force = false);

    static std::tuple<QString, bool> getPath(const QString & theme);
    static int getMode(const QString & theme);
signals:
    void themeChanged(QString themeName);
};

extern ThemeManager *themeManager;
