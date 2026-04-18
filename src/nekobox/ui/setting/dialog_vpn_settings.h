#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#ifndef NEKORAY_DIALOG_VPN_SETTINGS_H
#define NEKORAY_DIALOG_VPN_SETTINGS_H

#include <QDialog>
#include <QStringListModel>
#include "ui_dialog_vpn_settings.h"
#include "ui_dialog_app_settings.h"
#include <nekobox/ui/utils/MapListModel.hpp>

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogAppSettings;
    class DialogVPNSettings;
}
QT_END_NAMESPACE



class DialogAppSettings : public QDialog {
    Q_OBJECT
public:
    explicit DialogAppSettings(QWidget *parent = nullptr);
    
    ~DialogAppSettings() override;

    
    void on_button_box_clicked(QAbstractButton* button);


private:
    Ui::DialogAppSettings *ui;
    QStringListModel *proxy_list;
    QStringListModel *direct_list;
    QStringListModel *block_list;
};

class DialogVPNSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogVPNSettings(QWidget *parent = nullptr);

    ~DialogVPNSettings() override;

    void on_tun_applications_clicked();

    void on_exclude_cidrs_clicked();

private:
    Ui::DialogVPNSettings *ui;

public slots:

    void accept() override;
};

#endif // NEKORAY_DIALOG_VPN_SETTINGS_H
