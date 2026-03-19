#pragma once

#include <QObject>
<<<<<<< HEAD
=======
#include <QMap>
#include <QString>
>>>>>>> other-repo/main

class ThemeManager : public QObject {
    Q_OBJECT
public:
<<<<<<< HEAD
=======
    static QMap<QString, QString> & getThemes();
>>>>>>> other-repo/main
    QString system_style_name = "";
    QString current_theme = "0"; // int: 0:system 1+:builtin string: QStyleFactory

    void ApplyTheme(const QString &theme, bool force = false);
signals:
    void themeChanged(QString themeName);
};

extern ThemeManager *themeManager;
