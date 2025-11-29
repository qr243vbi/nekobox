#ifndef DIALOG_BASIC_SETTINGS_H
#define DIALOG_BASIC_SETTINGS_H

#include <QDialog>
#include <QJsonObject>
#include "ui_dialog_basic_settings.h"
#include "include/ui/mainwindow.h"


extern QList<QString> locales;

namespace Ui {
    class DialogBasicSettings;
}

class DialogBasicSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogBasicSettings(MainWindow *parent = nullptr);

    ~DialogBasicSettings();

    MainWindow * parent;

signals:
    void size_changed(int x, int y);
    void point_changed(int x, int y);

public slots:

    void accept();

private:
    Ui::DialogBasicSettings *ui;

    struct {
        QString custom_inbound;
        bool needRestart = false;
        bool updateDisableTray = false;
        bool updateSystemDns = false;
        bool updateIcon = false;
        bool updateFont = false;
    } CACHE;

private slots:
    void on_core_settings_clicked();
};

#endif // DIALOG_BASIC_SETTINGS_H
