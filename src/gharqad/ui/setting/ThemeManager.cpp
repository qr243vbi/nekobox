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

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
    }

    if (this->current_theme == theme && !force) {
        return;
    }

    auto lowerTheme = theme.toLower();
    if (lowerTheme == "system") {
        qApp->setStyleSheet("");
        qApp->setStyle(system_style_name);
    } else {
        QString path;
        {
            if (lowerTheme == "dark"){ path = ":nekobox/qss/MaterialDark.qss"; } 
            else if (lowerTheme == "amoled"){ path = ":nekobox/qss/AMOLED.qss"; } 
            else if (lowerTheme == "aqua") { path = ":nekobox/qss/Aqua.qss"; } 
            else if (lowerTheme == "aqua") { path = ":nekobox/qss/Aqua.qss"; } 
            else if (lowerTheme == "kawaii") { path = ":nekobox/qss/Kawaii.qss"; } 
    //        else if (lowerTheme == "elegantdark") { path = ":nekobox/qss/ElegantDark.qss"; } 
    //        else if (lowerTheme == "ubuntu") { path = ":nekobox/qss/Ubuntu.qss"; } 
    //        else if (lowerTheme == "flatdark") { path = ":nekobox/qss/FlatDark.qss"; } 
    //        else if (lowerTheme == "flatlight") { path = ":nekobox/qss/FlatLight.qss"; } 
    //        else if (lowerTheme == "materialdark") { path = ":nekobox/qss/MaterialDark.qss"; } 
    //        else if (lowerTheme == "manjaromix") { path = ":nekobox/qss/ManjaroMix.qss"; } 
     //       else if (lowerTheme == "consolestyles") { path = ":nekobox/qss/ConsoleStyles.qss"; } 
      //      else if (lowerTheme == "neonbuttons") { path = ":nekobox/qss/NeonButtons.qss"; } 
            else {
                qApp->setStyleSheet("");
                qApp->setStyle(theme);
                goto skip_set_style;
            }
        }
        QFile f(path);
        if (f.open(QFile::ReadOnly | QFile::Text)){
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
        }

        
    }
    skip_set_style:
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
