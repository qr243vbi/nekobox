
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <QDir>
#include "nekobox/ui/security_addon.h"
#include "nekobox/dataStore/Utils.hpp"
#include "nekobox/ui/mainwindow.h"
#include "nekobox/ui/security/security.h"
#include <QAction>
#include <QCryptographicHash>
#include <QSettings>
#include <QtTranslation>
#include <qlineedit.h>
#include <qnamespace.h>
#include <qpushbutton.h>


QSettings *local_keys = nullptr;
QByteArray default_password;
QStringList userlist;

void set_access_denied(QWidget * w){
  auto l1 = new QVBoxLayout();
  auto l2 = new QLabel(QObject::tr("Access denied"));
  auto l3 = new QPushButton(QObject::tr("OK"));
  l1->addWidget(l2); l1->addWidget(l3); w->setLayout(l1);
  QObject::connect(l3, &QPushButton::clicked, w, [w] {
    w->close();
  });
};

static QByteArray hashPassword(const QString &password)
{
    return QCryptographicHash::hash(
        password.toUtf8(),
        QCryptographicHash::Sha256
    );
}

class SimpleListModel : public QAbstractListModel {
public:
  explicit SimpleListModel(QObject *parent = nullptr)
      : QAbstractListModel(parent) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return userlist.size();
  }

  QVariant data(const QModelIndex &index, int role) const override {
    if (!index.isValid())
      return QVariant();

    const auto &item = userlist.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
      return item;
    default:
      return QVariant();
    }
  }

  bool setData(const QModelIndex &index, const QVariant &value,
               int role) override {
    return false;
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override {
    if (!index.isValid()) {
      return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }
};


 long long time_startup = 0;
 long long time_settings = 0;
 long long time_systray = 0;
 long long time_line = 0;
 long long time_type = 0;


static long long * getTimePointer(LockValue val){
  switch (val){
    case LockValue::LockSettings:
    return &time_settings;
    case LockValue::LockStartup:
    return &time_startup;
    case LockValue::LockSystray:
    return &time_systray;
    default: 
    return nullptr;
  }
}

bool confirmLock(LockValue val, bool restart){
  init_keys();
  if (userlist.isEmpty()){
    return true;
  }

  bool ret = !(getLocked(val));
  if (ret) return true;

  auto seconds = QDateTime::currentSecsSinceEpoch();
  long long * ptr = getTimePointer(val);
  if (ptr == nullptr) return false;
  if (*ptr > seconds) return true;

  auto confirm = new ConfirmForm();
  confirm->ui->comboBox->setCurrentIndex(time_type);
  confirm->ui->spinBox->setValue(time_line);
  if (restart){
    confirm->ui->spinBox->setValue(999999);
    confirm->ui->label_group->hide();
  }
  QObject::connect(confirm->ui->buttonBox, &QDialogButtonBox::rejected, confirm, [confirm](){
    confirm->close();
  });

  QObject::connect(confirm->ui->buttonBox, &QDialogButtonBox::accepted, confirm, [val, confirm, &ret, ptr, restart](){
    auto username = confirm->ui->username->text();
    QString password;
    bool checked;
    if (!userlist.contains(username)) {
      qDebug() << "Username " << username << " is not exists";
      goto skip_timing;
    }
    password = confirm->ui->password->text();
    checked = checkPassword(username, password);
    if (!checked) {
      qDebug() << "passwords does not match";
      goto skip_timing;
    }
    ret = !(getLocked(val, username));
    if (ret){
      int seconds = confirm->ui->spinBox->text().toInt();
      if (!restart) time_line = seconds;
      if (seconds <= 0) goto skip_timing; 
      int curind = confirm->ui->comboBox->currentIndex();
      for (int n = curind; n > 0; n --){
        seconds *= 60;
      }
      if (!restart) time_type = curind; 
      *ptr = QDateTime::currentSecsSinceEpoch() + seconds;
    }
    skip_timing:
    confirm->close();
    return true;
  });

  confirm->show();
  confirm->exec();
  return ret;
};


class CheckableListModel : public QAbstractListModel {

public:
  QMap<QString, bool> SelectedList;

  explicit CheckableListModel(QObject *parent = nullptr)
      : QAbstractListModel(parent) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return userlist.size();
  }

  QVariant data(const QModelIndex &index, int role) const override {
    if (!index.isValid())
      return QVariant();

    const auto &item = userlist.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
      return item;
    case Qt::CheckStateRole:
      return SelectedList[item] ? Qt::Checked : Qt::Unchecked;
    default:
      return QVariant();
    }
  }

  bool setData(const QModelIndex &index, const QVariant &value,
               int role) override {
    if (!index.isValid()) {
      return false;
    }

    auto &item = userlist[index.row()];

    if (role == Qt::CheckStateRole) {
      bool item_checked = (value.toInt() == Qt::Checked);
      SelectedList[item] = item_checked;
      emit dataChanged(index, index, {Qt::CheckStateRole});
      return true;
    }

    return false;
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override {
    if (!index.isValid()) {
      return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
  }
};

void init_keys() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  local_keys = new QSettings(KEYS_INI_PATH, QSettings::IniFormat);
  default_password = hashPassword(NKR_DEFAULT_PASSWORD);
  if (local_keys->contains("userlist")) {
    userlist = local_keys->value("userlist", "").toStringList();
  }
}

static QString getLockPart(LockValue key) {
  init_keys();
  QString part = "";
  switch (key) {
  case LockValue::LockSettings:
    part = "settings";
    break;
  case LockValue::LockStartup:
    part = "startup";
    break;
  case LockValue::LockSystray:
    part = "systray";
    break;
  }
  return part;
}

bool getLocked(LockValue key, const QString &username) {
  init_keys();
  QString part = getLockPart(key);
  if (part == "")
    return false;
  if (username != "")
    part = "lock_" + part + "_" + username;
  else
    part = "lock_" + part;
  return local_keys->value(part, false).toBool();
};

void setInboundPassword(const QString &username, const QString &password) {
  init_keys();
  Configs::dataStore->inbound_password = password;
  Configs::dataStore->inbound_username = username;
}

void setLocked(LockValue key, bool value, const QString &username) {
  init_keys();
  QString part = getLockPart(key);
  if (part == "")
    return;
  if (username != "")
    part = "lock_" + part + "_" + username;
  else
    part = "lock_" + part;
  local_keys->setValue(part, value);
  local_keys->sync();
};

void addUser(const QString &username) {
  init_keys();
  if (!userlist.contains(username)) {
    userlist << username;
    local_keys->setValue("userlist", userlist);
    local_keys->sync();
  }
};

QByteArray getPasswordHash(const QString &username) {
  init_keys();
  QString user = "user_" + username;
  if (local_keys->contains(user)) {
    return local_keys->value(user, "").toByteArray();
  } else {
    return default_password;
  }
};

bool checkPassword(const QString &username, const QString &password) {
  init_keys();
  if (userlist.isEmpty()){
    return true;
  }
  QByteArray oldhash = getPasswordHash(username);
  QByteArray newhash = hashPassword(password);
  return oldhash == newhash;
};

void delUser(const QString &username) {
  init_keys();
  if (userlist.contains(username)) {
    userlist.removeAt(userlist.indexOf(username));
    local_keys->setValue("userlist", userlist);
    local_keys->sync();
  }
  QString part = "user_" + username;
  if (local_keys->contains(part)) {
    local_keys->remove(part);
  }
};

void setPassword(const QString &username, const QString &password) {
  init_keys();
  if (password == "") {
    delUser(username);
    return;
  }
  addUser(username);
  local_keys->setValue(
      "user_" + username,
      hashPassword(password));
  local_keys->sync();
};

void modify_security_action(MainWindow *win, QAction *sec) {
  init_keys();
  sec->setText(QAction::tr("Security Settings"));

  QObject::connect(sec, &QAction::triggered, win, [win]() {
    if (confirmLock(LockValue::LockSettings)){
      auto sec = new SecurityForm(false, win);
      sec->show();
    }
  });
}

ConfirmForm::ConfirmForm(QWidget *parent) {
  ui = new Ui::ConfirmForm();
  ui->setupUi(this);
}

PasswordForm::PasswordForm(QWidget *parent) {
  ui = new Ui::PasswordForm();
  ui->setupUi(this);
}

UsersForm::UsersForm(QWidget *parent) {
  ui = new Ui::UsersForm();
  ui->setupUi(this);
}

SecurityForm::SecurityForm(bool is_user_defined, QWidget *parent) {
  ui = new Ui::SecurityForm();
  ui->setupUi(this);
//  ui->auth_startup->hide();
//  ui->lock_system_tray->hide();

  if (is_user_defined) {
    ui->button_group->hide();
    ui->groupBox->hide();
    return;
  }

  ui->auth_startup->setChecked(getLocked(LockValue::LockStartup));
  ui->encrypt_settings->setChecked(getLocked(LockValue::LockSettings));
  ui->lock_system_tray->setChecked(getLocked(LockValue::LockSystray));

  QObject::connect(
      ui->reset_settings, &QPushButton::clicked, this, [this]() -> void {
        auto users = new UsersForm();
        auto model = new CheckableListModel();
        users->ui->listView->setModel(model);
        QObject::connect(users->ui->buttonBox, &QDialogButtonBox::rejected,
                         this, [users]() -> void { users->close(); });
        QObject::connect(
            users->ui->buttonBox, &QDialogButtonBox::accepted, this,
            [users, model]() -> void {
              users->close();
              for (auto [key, value] : asKeyValueRange(model->SelectedList)) {
                if (value) {
                  delUser(key);
                }
              }
            },
            Qt::SingleShotConnection);
        users->show();
      });

  QObject::connect(
      ui->edit_users, &QPushButton::clicked, this, [this]() -> void {
        auto users = new UsersForm();
        auto model = new SimpleListModel();
        users->ui->listView->setModel(model);
        users->ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok);

        QObject::connect(
            users->ui->listView, &QListView::doubleClicked, this,
            [users, model](const QModelIndex &index) -> void {
              if (!index.isValid()) {
                return;
              }
              QString text = index.data(Qt::DisplayRole).toString();
              if (text != "") {
                auto sec = new SecurityForm(true, users);
                auto buttons = new QDialogButtonBox();
                buttons->setStandardButtons(QDialogButtonBox::Ok |
                                            QDialogButtonBox::Cancel);
                auto ui = sec->ui;
                ui->auth_startup->setChecked(
                    getLocked(LockValue::LockStartup, text));
                ui->encrypt_settings->setChecked(
                    getLocked(LockValue::LockSettings, text));
                ui->lock_system_tray->setChecked(
                    getLocked(LockValue::LockSystray, text));
                QObject::connect(
                    buttons, &QDialogButtonBox::rejected, sec,
                    [sec]() -> void { sec->close(); },
                    Qt::SingleShotConnection);
                QObject::connect(
                    buttons, &QDialogButtonBox::accepted, sec,
                    [sec, text]() -> void {
                      bool lock_settings, lock_system_tray, lock_startup;
                      lock_startup = sec->ui->auth_startup->isChecked();
                      lock_system_tray = sec->ui->lock_system_tray->isChecked();
                      lock_settings = sec->ui->encrypt_settings->isChecked();
                      setLocked(LockValue::LockStartup, lock_startup, text);
                      setLocked(LockValue::LockSystray, lock_system_tray, text);
                      setLocked(LockValue::LockSettings, lock_settings, text);
                      sec->close();
                    },
                    Qt::SingleShotConnection);
                sec->layout()->addWidget(buttons);
                sec->show();
              }
            });

        QObject::connect(
            users->ui->buttonBox, &QDialogButtonBox::accepted, this,
            [users]() -> void { users->close(); }, Qt::SingleShotConnection);
        users->show();
      });

  QObject::connect(ui->auth_startup, &QCheckBox::clicked, this,
                   [this]() -> void {
                     int checkstate = ui->auth_startup->checkState();
                     setLocked(LockValue::LockStartup,
                               checkstate == Qt::CheckState::Checked);
                   });

  QObject::connect(ui->encrypt_settings, &QCheckBox::clicked, this,
                   [this]() -> void {
                     int checkstate = ui->encrypt_settings->checkState();
                     setLocked(LockValue::LockSettings,
                               checkstate == Qt::CheckState::Checked);
                   });

  QObject::connect(ui->lock_system_tray, &QCheckBox::clicked, this,
                   [this]() -> void {
                     int checkstate = ui->lock_system_tray->checkState();
                     setLocked(LockValue::LockSystray,
                               checkstate == Qt::CheckState::Checked);
                   });

  QObject::connect(
      ui->change_proxy_password, &QPushButton::clicked, this, [this]() -> void {
        auto sec = new PasswordForm(this);
        QObject::connect(
            sec->ui->buttonBox, &QDialogButtonBox::accepted, this,
            [sec, this]() -> void {
              QString password = sec->ui->confpass->text();
              if (password == sec->ui->newpass->text()) {
                QString username = sec->ui->curpass->text();
                setInboundPassword(username, password);
              }
              sec->close();
            },
            Qt::SingleShotConnection);
        QObject::connect(
            sec->ui->buttonBox, &QDialogButtonBox::rejected, this,
            [this, sec]() -> void { sec->close(); }, Qt::SingleShotConnection);
        sec->show();
      });

  QObject::connect(
      ui->change_ui_password, &QPushButton::clicked, this, [this]() -> void {
        auto sec = new PasswordForm(this);
        QObject::connect(
            sec->ui->buttonBox, &QDialogButtonBox::accepted, this,
            [sec, this]() -> void {
              QString password = sec->ui->confpass->text();
              if (password == sec->ui->newpass->text()) {
                QString username = sec->ui->curpass->text();
                if (username == "") {
                  username = NKR_DEFAULT_USERNAME;
                }
                setPassword(username, password);
              }
              sec->close();
            },
            Qt::SingleShotConnection);
        QObject::connect(
            sec->ui->buttonBox, &QDialogButtonBox::rejected, this,
            [this, sec]() -> void { sec->close(); }, Qt::SingleShotConnection);
        sec->show();
      });
}