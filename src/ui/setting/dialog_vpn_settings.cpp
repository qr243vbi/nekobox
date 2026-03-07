#include <QTextEdit>
#include "nekobox/ui/setting/dialog_vpn_settings.h"

#include "nekobox/configs/ConfigBuilder.hpp"
#include "nekobox/configs/proxy/Preset.hpp"
#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/global/GuiUtils.hpp"
#include "nekobox/ui/mainwindow_interface.h"
#ifdef Q_OS_WIN
#include "nekobox/sys/windows/WinVersion.h"
#endif

#include <QMessageBox>
#define ADJUST_SIZE                                                            \
  runOnThread(                                                                 \
      [=, this] {                                                              \
        adjustSize();                                                          \
        adjustPosition(mainwindow);                                            \
      },                                                                       \
      this);
DialogVPNSettings::DialogVPNSettings(QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogVPNSettings) {
  CHECK_SETTINGS_ACCESS
  ui->setupUi(this);
  ADD_ASTERISK(this);

  ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
  ui->vpn_implementation->setCurrentText(
      Configs::dataStore->vpn_implementation);
  auto lambda = [this](int state) {
    bool hidden = (state != Qt::Checked);
    ui->label_3->setHidden(hidden);
    ui->tun_address_6->setHidden(hidden);
  };
  connect(ui->exclude_cidrs, &QPushButton::clicked, this, []() {
    auto dialog = new QDialog();
    dialog->setWindowTitle(QObject::tr("Exclude CIDR's"));
    dialog->resize(400, 300);

    // Create the text edit area
    QTextEdit *textEdit = new QTextEdit(dialog);
    textEdit->setPlainText(Configs::dataStore->route_exclude_addrs.join("\n"));

    // Create the OK and Cancel buttons
    QPushButton *okButton = new QPushButton(QObject::tr("OK"), dialog);
    QPushButton *cancelButton = new QPushButton(QObject::tr("Cancel"), dialog);

    // Create layout
    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
    mainLayout->addWidget(textEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    QObject::connect(okButton, &QPushButton::clicked, [textEdit, dialog]() {
      QStringList text = textEdit->toPlainText().split("\n");
      auto & addrs = Configs::dataStore->route_exclude_addrs;
      addrs.clear();
      for (auto str : text){
        str = str.trimmed();
        if (!str.isEmpty()){
          addrs << str;
        }
      }
      dialog->accept(); // Close the dialog and return QDialog::Accepted
    });

    QObject::connect(cancelButton, &QPushButton::clicked, [&]() {
      dialog->reject(); // Close the dialog and return QDialog::Rejected
    });
    dialog->show();
    // Show dialog and run the event loop
    dialog->exec();
  });
  connect(ui->vpn_ipv6, &QCheckBox::checkStateChanged, this, lambda);
  bool ipv6;
  ui->vpn_mtu->setCurrentText(QString::number(Configs::dataStore->vpn_mtu));
  ui->vpn_ipv6->setChecked(ipv6 = Configs::dataStore->vpn_ipv6);

  ui->strict_route->setChecked(Configs::dataStore->vpn_strict_route);
  ui->tun_routing->setChecked(Configs::dataStore->enable_tun_routing);
  ui->auto_redirect->hide();
  //   ui->auto_redirect->setChecked(Configs::dataStore->auto_redirect);
  ui->tun_address->setText(Configs::getTunAddress());
  ui->tun_address_6->setText(Configs::getTunAddress6());
  ADJUST_SIZE

  if (!ipv6) {
    lambda(Qt::Unchecked);
  }
}

DialogVPNSettings::~DialogVPNSettings() { delete ui; }

void DialogVPNSettings::accept() {
  //
  auto mtu = ui->vpn_mtu->currentText().toInt();
  if (mtu > 10000 || mtu < 1000)
    mtu = 9000;
  Configs::dataStore->vpn_implementation =
      ui->vpn_implementation->currentText();
  Configs::dataStore->vpn_mtu = mtu;
  bool ipv6 = Configs::dataStore->vpn_ipv6 = ui->vpn_ipv6->isChecked();
  Configs::dataStore->vpn_strict_route = ui->strict_route->isChecked();
  Configs::dataStore->enable_tun_routing = ui->tun_routing->isChecked();
  if (ipv6) {
    Configs::dataStore->tun_address_6 = ui->tun_address_6->text();
  }
  Configs::dataStore->tun_address = ui->tun_address->text();
  //   Configs::dataStore->auto_redirect = ui->auto_redirect->isChecked();
  //   Configs::dataStore->tun_name = ui->tun_name->text();
  //
  QStringList msg{"UpdateDataStore"};
  msg << "VPNChanged";
  MW_dialog_message("", msg.join(","));
  QDialog::accept();
}
