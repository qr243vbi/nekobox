#include <QStyle>
#include <QApplication>
#include <QStyleFactory>

#include "ThemeManager.hpp"
#include "iostream"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
    }

    if (this->current_theme == theme && !force) {
        return;
    }

    if (theme.toLower() == "system") {
        qApp->setStyle(system_style_name);
    } else {
        qApp->setStyle(theme);
    }

    current_theme = theme;

    emit themeChanged(theme);
}
