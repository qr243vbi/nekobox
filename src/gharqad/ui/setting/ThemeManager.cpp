#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <QStyle>
#include <QApplication>
#include <QFile>
#include <QPalette>

#include "nekobox/ui/setting/ThemeManager.hpp"
#include "nekobox/dataStore/Utils.hpp"

ThemeManager *themeManager = new ThemeManager;

QMap<QString, QString> & ThemeManager::getThemes(){
    static QMap<QString, QString> map;
    static bool initialized = false;
    if (!initialized){
        initialized = true;
    //    ReadFileText(":nekobox/qss/styles.list");
    }
    return map;
}


std::tuple<QString, bool> ThemeManager::getPath(const QString & theme){
    auto lowerTheme = theme.toLower();
    if (lowerTheme == "system"){ return std::make_tuple(qApp->style()->name(), false); } 
    else if (lowerTheme == "dark"){ return std::make_tuple(":nekobox/qss/MaterialDark.qss", true); } 
    else if (lowerTheme == "amoled"){ return std::make_tuple(":nekobox/qss/AMOLED.qss", true); } 
    else if (lowerTheme == "aqua") { return std::make_tuple(":nekobox/qss/Aqua.qss", true); } 
    else if (lowerTheme == "kawaii") { return std::make_tuple(":nekobox/qss/Kawaii.qss", true); } 
    else if (lowerTheme == "flatgray") { return std::make_tuple(":nekobox/qss/flatgray.css", true); } 
    else if (lowerTheme == "lightblue") { return std::make_tuple(":nekobox/qss/lightblue.css", true); } 
    else if (lowerTheme == "blacksoft") { return std::make_tuple(":nekobox/qss/blacksoft.css", true); } 
    else { return std::make_tuple(theme, false); }
}

int ThemeManager::getMode(const QString & theme){
    auto lowerTheme = theme.toLower();
    if (lowerTheme == "system"){ return 0; } 
    else if (lowerTheme == "dark"){ return 1; } 
    else if (lowerTheme == "amoled"){ return 1; } 
    else if (lowerTheme == "aqua") { return 1; } 
    else if (lowerTheme == "kawaii") { return 2; } 
    else if (lowerTheme == "flatgray") { return 2; } 
    else if (lowerTheme == "lightblue") { return 2; } 
    else if (lowerTheme == "blacksoft") { return 1; } 
    else if (lowerTheme == "windowsvista") { return 2; } 
    else { return 0; }
}

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (!force){
        if (theme == current_theme){
            return;
        }
    }
    
    QString lower_theme;
    bool isFile;
    std::tie(lower_theme, isFile) = getPath(theme);

    if (!isFile) {
        qApp->setStyleSheet("");
        qApp->setStyle(lower_theme);
    } else {
        QFile f(lower_theme);
        if (f.open(QFile::ReadOnly | QFile::Text)){
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
        }
    }
/*
    {
    QString style = qApp->styleSheet();

  qApp->setStyleSheet(style + R"(
    QGroupBox[flat="true"] {
        border: none;
        background: transparent;
    }      
    QGroupBox[rounded="true"] {
        border-radius: 10px; border: 3px solid palette(midlight);
        margin-top: 0px;
    }
  )");
    }
*/
    current_theme = theme;

    emit themeChanged(theme);
}
