#include <qnamespace.h>
#ifdef _WIN32
#include <winsock2.h>
#endif

#include <QTextEdit>
#include <nekobox/ui/setting/dialog_vpn_settings.h>

#include <nekobox/configs/ConfigBuilder.hpp>
#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/dataStore/Configs.hpp>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/ui/mainwindow_interface.h>
#ifdef Q_OS_WIN
#include <nekobox/sys/windows/WinVersion.h>
#endif
#include <nekobox/global/GuiUtils.hpp>

#include <QtGlobal> // For QT_VERSION_CHECK
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#define STATE_CHANGED &QCheckBox::checkStateChanged
#else
#define STATE_CHANGED &QCheckBox::stateChanged
#endif

#include <QMessageBox>
#define ADJUST_SIZE                                                            \
  runOnThread(                                                                 \
      [=, this] {                                                              \
        adjustSize();                                                          \
        adjustPosition(mainwindow);                                            \
      },                                                                       \
      this);

void DialogAppSettings::on_button_box_clicked(QAbstractButton *button) {

  auto role = ui->buttonBox->buttonRole(button);

  switch (role) {
  case QDialogButtonBox::AcceptRole:
    if (button != ui->buttonBox->button(QDialogButtonBox::Ok)) {
      goto loop1;
    } else {
// OK
#ifdef DEBUG_MODE
      qDebug() << "OK clicked";
#endif
      auto split = Configs::dataStore->routing->tun_split;
      split->block = block_list->stringList();
      split->proxy = proxy_list->stringList();
      split->direct = direct_list->stringList();
      Configs::dataStore->routing->Save();
      this->accept();
      break;
    }

  case QDialogButtonBox::RejectRole: {
    this->reject();
    break;
  }
  case QDialogButtonBox::DestructiveRole:
// Discard behaves like reject
#ifdef DEBUG_MODE
    qDebug() << "Discard clicked";
#endif
    {
      int index = this->ui->tun_apps->currentIndex();
      QListView *view;
      switch (index) {
      case 0:
        view = ui->proxy_tun_apps;
        break;
      case 1:
        view = ui->direct_tun_apps;
        break;
      case 2:
        view = ui->block_tun_apps;
        break;
      default:
        return;
      }
      QModelIndexList indexes = view->selectionModel()->selectedIndexes();
      auto model = view->model();

      std::sort(indexes.begin(), indexes.end(),
                [](const QModelIndex &a, const QModelIndex &b) {
                  return a.row() > b.row();
                });

      for (const QModelIndex &index : indexes) {
        model->removeRows(index.row(), 1);
      }
    }
    break;

  case QDialogButtonBox::ActionRole:
  loop1:
// Open usually falls here
#ifdef DEBUG_MODE
    qDebug() << "Open clicked";
#endif
#define insert_path(X)                                                         \
  {                                                                            \
    model = X##_list;                                                          \
  };
    {
      QStringListModel *model;
      auto path = OPEN_FILENAME;
      if (path != "") {
        int index = this->ui->tun_apps->currentIndex();

        switch (index) {
        case 0: {
          insert_path(proxy)
#ifdef DEBUG_MODE
                  qDebug()
              << "Add" << path << "to index" << index;
#endif
        } break;
        case 1: {
          insert_path(direct)
#ifdef DEBUG_MODE
                  qDebug()
              << "Add" << path << "to index" << index;
#endif
        } break;
        case 2: {
          insert_path(block)
#ifdef DEBUG_MODE
                  qDebug()
              << "Add" << path << "to index" << index;
#endif
        } break;
        default:
          return;
        }
      }
      int row = model->rowCount();
      model->insertRow(row);
      model->setData(model->index(row), path);
    }
    break;
#undef insert_path
  case QDialogButtonBox::ResetRole:
#ifdef DEBUG_MODE
    qDebug() << "Reset clicked";
#endif
    {
      proxy_list->setStringList(Configs::dataStore->routing->tun_split->proxy);
      direct_list->setStringList(
          Configs::dataStore->routing->tun_split->direct);
      block_list->setStringList(Configs::dataStore->routing->tun_split->block);
    }
    break;

  default:
    break;
  }
};

static QVariant getAppPath(const QString &value, int role) {
  if (role == ACCEPT_DATA_ROLE || role == Qt::DisplayRole) {
    return value; // QDir::cleanPath(value);
  } else {
    return "";
  }
}

DialogAppSettings::DialogAppSettings(QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogAppSettings) {
  this->setAttribute(Qt::WA_DeleteOnClose);
  ui->setupUi(this);

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this,
          &DialogAppSettings::on_button_box_clicked);

  disconnect(ui->buttonBox, &QDialogButtonBox::accepted, this,
             &QDialog::accept);

  this->setWindowTitle(QObject::tr("Application Rules"));

  proxy_list =
      new QStringListModel(Configs::dataStore->routing->tun_split->proxy);
  direct_list =
      new QStringListModel(Configs::dataStore->routing->tun_split->direct);
  block_list =
      new QStringListModel(Configs::dataStore->routing->tun_split->block);

  ui->proxy_tun_apps->setModel(proxy_list);
  ui->proxy_tun_apps->setSelectionMode(QAbstractItemView::ExtendedSelection);
  ui->direct_tun_apps->setModel(direct_list);
  ui->proxy_tun_apps->setSelectionMode(QAbstractItemView::ExtendedSelection);
  ui->block_tun_apps->setModel(block_list);
  ui->proxy_tun_apps->setSelectionMode(QAbstractItemView::ExtendedSelection);
};

DialogAppSettings::~DialogAppSettings() {

};

void DialogVPNSettings::on_exclude_cidrs_clicked() {
  auto dialog = new QDialog();
  dialog->setWindowTitle(QObject::tr("Exclude CIDR's"));
  dialog->resize(400, 300);
  dialog->setAttribute(Qt::WA_DeleteOnClose);

  // Create the text edit area
  QTextEdit *textEdit = new QTextEdit(dialog);
  textEdit->setPlainText(Configs::dataStore->route_exclude_addrs.join("\n"));

  // Create the OK and Cancel buttons
  QPushButton *okButton = new QPushButton(QObject::tr("OK"), dialog);
  QPushButton *cancelButton = new QPushButton(QObject::tr("Cancel"), dialog);

  // Create layout
  QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
  mainLayout->addWidget(textEdit);

  QHBoxLayout *buttonLayout = new QHBoxLayout(dialog);
  buttonLayout->addStretch();
  buttonLayout->addWidget(okButton);
  buttonLayout->addWidget(cancelButton);
  mainLayout->addLayout(buttonLayout);

  QObject::connect(okButton, &QPushButton::clicked, [textEdit, dialog]() {
    QStringList text = textEdit->toPlainText().split("\n");
    auto &addrs = Configs::dataStore->route_exclude_addrs;
    addrs.clear();
    for (auto str : text) {
      str = str.trimmed();
      if (!str.isEmpty()) {
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
}

void DialogVPNSettings::on_tun_applications_clicked() {
  auto dialog = new DialogAppSettings(this);
  dialog->exec();
}

DialogVPNSettings::DialogVPNSettings(QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogVPNSettings) {
  CHECK_SETTINGS_ACCESS
  ui->setupUi(this);
  ADD_ASTERISK(this);
  this->setAttribute(Qt::WA_DeleteOnClose);

  ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
  ui->vpn_implementation->setCurrentText(
      Configs::dataStore->vpn_implementation);
  auto lambda = [this](int state) {
    bool hidden = (state != Qt::Checked);
    ui->label_3->setHidden(hidden);
    ui->tun_address_6->setHidden(hidden);
  };
  connect(ui->tun_applications, &QPushButton::clicked, this,
          &DialogVPNSettings::on_tun_applications_clicked);
  connect(ui->exclude_cidrs, &QPushButton::clicked, this,
          &DialogVPNSettings::on_exclude_cidrs_clicked);
  connect(ui->vpn_ipv6, STATE_CHANGED, this, lambda);
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
