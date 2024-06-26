#include <QStyle>
#include <QApplication>
#include <QStyleFactory>

#include "ThemeManager.hpp"
#include "iostream"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

void ThemeManager::ApplyTheme(const QString &theme) {
    auto internal = [=] {

        if (this->system_style_name.isEmpty()) {
            this->system_style_name = qApp->style()->name();
        }

        if (this->current_theme == theme) {
            return;
        }

        if (theme.toLower() == "system") {
            qApp->setStyle(system_style_name);
        } else {
            qApp->setStyle(theme);
        }

        current_theme = theme;
    };
    internal();

    auto nekoray_css = ReadFileText(":/neko/neko.css");
    qApp->setStyleSheet(qApp->styleSheet().append("\n").append(nekoray_css));
}
