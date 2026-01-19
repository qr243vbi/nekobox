#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "nekobox/ui/setting/dialog_vpn_settings.h"

#include "nekobox/configs/proxy/Preset.hpp"
#include "nekobox/global/GuiUtils.hpp"
#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/configs/ConfigBuilder.hpp"
#include "nekobox/ui/mainwindow_interface.h"
#ifdef Q_OS_WIN
#include "nekobox/sys/windows/WinVersion.h"
#endif

#include <QMessageBox>
#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);
DialogVPNSettings::DialogVPNSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogVPNSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);
/*
#ifdef Q_OS_WIN
    if (WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_10_1507)) {
        ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
        ui->vpn_implementation->setCurrentText(Configs::dataStore->vpn_implementation);
    }
    else {
        ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
        ui->vpn_implementation->setCurrentText("gvisor");
        ui->vpn_implementation->setEnabled(false);
    }
#else
*/
    ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
    ui->vpn_implementation->setCurrentText(Configs::dataStore->vpn_implementation);
//#endif
    connect(ui->vpn_ipv6, &QCheckBox::checkStateChanged, this, [this](int state) {
        bool hidden = (state != Qt::Checked);
        ui->label_3->setHidden(hidden);
        ui->tun_address_6->setHidden(hidden);
    });

    ui->vpn_mtu->setCurrentText(QString::number(Configs::dataStore->vpn_mtu));
    ui->vpn_ipv6->setChecked(Configs::dataStore->vpn_ipv6);
    ui->strict_route->setChecked(Configs::dataStore->vpn_strict_route);
    ui->tun_routing->setChecked(Configs::dataStore->enable_tun_routing);
    ui->auto_redirect->hide();
 //   ui->auto_redirect->setChecked(Configs::dataStore->auto_redirect);
    ui->tun_name->setText(Configs::getTunName());
    ui->tun_address->setText(Configs::getTunAddress());
    ui->tun_address_6->setText(Configs::getTunAddress6());
    ADJUST_SIZE
}

DialogVPNSettings::~DialogVPNSettings() {
    delete ui;
}

void DialogVPNSettings::accept() {
    //
    auto mtu = ui->vpn_mtu->currentText().toInt();
    if (mtu > 10000 || mtu < 1000) mtu = 9000;
    Configs::dataStore->vpn_implementation = ui->vpn_implementation->currentText();
    Configs::dataStore->vpn_mtu = mtu;
    bool ipv6 = Configs::dataStore->vpn_ipv6 = ui->vpn_ipv6->isChecked();
    Configs::dataStore->vpn_strict_route = ui->strict_route->isChecked();
    Configs::dataStore->enable_tun_routing = ui->tun_routing->isChecked();
    if (ipv6){
        Configs::dataStore->tun_address_6 = ui->tun_address_6->text();
    }
    Configs::dataStore->tun_address = ui->tun_address->text();
 //   Configs::dataStore->auto_redirect = ui->auto_redirect->isChecked();
    Configs::dataStore->tun_name = ui->tun_name->text();
    //
    QStringList msg{"UpdateDataStore"};
    msg << "VPNChanged";
    MW_dialog_message("", msg.join(","));
    QDialog::accept();
}

void DialogVPNSettings::on_troubleshooting_clicked() {


    QMessageBox msg(
        QMessageBox::Information,
        tr("Troubleshooting"),
        tr("If you have trouble starting VPN, you can force reset Core process here.\n\n"
            "If still not working, see documentation for more information.\n"
            "https://matsuridayo.github.io/n-configuration/#vpn-tun"),
        QMessageBox::NoButton,
        this
    );
    msg.addButton(tr("Reset"), QMessageBox::ActionRole);
    auto cancel = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);

    msg.setDefaultButton(cancel);
    msg.setEscapeButton(cancel);

    auto r = msg.exec() - 2;
    if (r == 0) {
        GetMainWindow()->StopVPNProcess();
    }
}
