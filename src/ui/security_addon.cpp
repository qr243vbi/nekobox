#pragma once
#include "nekobox/ui/security_addon.h"
#include "nekobox/dataStore/Utils.hpp"
#include "nekobox/ui/mainwindow.h"
#include "nekobox/ui/security/security.h"
#include <QAction>
#include <QCryptographicHash>
#include <QSettings>
#include <QtTranslation>
#include <qlineedit.h>
#include <qpushbutton.h>

#ifndef NKR_DEFAULT_PASSWORD
#define NKR_DEFAULT_PASSWORD NKR_SOFTWARE_KEYS
#endif

#ifndef NKR_DEFAULT_USERNAME
#define NKR_DEFAULT_USERNAME "admin"
#endif

QSettings *local_keys = nullptr;
QByteArray default_password;
QStringList userlist;

void init_keys() {
  static bool initialized = false;
  if (initialized)
    return;
  initialized = true;
  local_keys = new QSettings(KEYS_INI_PATH, QSettings::IniFormat);
  default_password = QCryptographicHash::hash(NKR_DEFAULT_PASSWORD,
                                              QCryptographicHash::Sha256);
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
  Configs::dataStore->inbound_password = password;
  Configs::dataStore->inbound_username = username;
}

void setLocked(LockValue key, bool value, const QString &username) {
  QString part = getLockPart(key);
  if (part == "")
    return;
  if (username != "")
    part = "lock_" + part + "_" + username;
  else
    part = "lock_" + part;
  qDebug() << part << value;
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
  if (local_keys->contains(username)) {
    return local_keys->value("user_" + username, "").toByteArray();
  } else {
    return default_password;
  }
};

bool checkPassword(const QString &username, const QString &password) {
  auto oldhash = getPasswordHash(username);
  auto newhash =
      QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
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
  if (password == "") {
    delUser(username);
    return;
  }
  addUser(username);
  local_keys->setValue(
      "user_" + username,
      QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256));
  local_keys->sync();
};

static inline void modify_security_action(MainWindow *win, QAction *sec) {
  sec->setText(QAction::tr("Security Settings"));

  QObject::connect(sec, &QAction::triggered, win, [win]() {
    auto sec = new SecurityForm(win);
    sec->show();
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

SecurityForm::SecurityForm(QWidget *parent) {
  ui = new Ui::SecurityForm();
  ui->setupUi(this);

  ui->auth_startup->setChecked(getLocked(LockValue::LockStartup));
  ui->encrypt_settings->setChecked(getLocked(LockValue::LockSettings));
  ui->lock_system_tray->setChecked(getLocked(LockValue::LockSystray));

  QObject::connect(ui->reset_settings, &QPushButton::clicked, this,
                   [this]() -> void {
                     auto users = new UsersForm();
                     users->show();
                   });

  QObject::connect(ui->edit_users, &QPushButton::clicked, this,
                   [this]() -> void {
                     auto users = new UsersForm();
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
      ui->change_proxy_password, &QPushButton::clicked, this, [this]() {
        auto sec = new PasswordForm(this);
        QObject::connect(sec->ui->buttonBox, &QDialogButtonBox::accepted, this,
                         [sec, this]() -> void {
                           QString password = sec->ui->confpass->text();
                           if (password == sec->ui->newpass->text()) {
                             QString username = sec->ui->curpass->text();
                             setInboundPassword(username, password);
                           }
                           sec->close();
                         });
        QObject::connect(sec->ui->buttonBox, &QDialogButtonBox::rejected, this,
                         [this, sec]() -> void { sec->close(); });
        sec->show();
      });

  QObject::connect(
      ui->change_ui_password, &QPushButton::clicked, this, [this]() {
        auto sec = new PasswordForm(this);
        QObject::connect(sec->ui->buttonBox, &QDialogButtonBox::accepted, this,
                         [sec, this]() -> void {
                           QString password = sec->ui->confpass->text();
                           if (password == sec->ui->newpass->text()) {
                             QString username = sec->ui->curpass->text();
                             if (username == "")
                               username = NKR_DEFAULT_USERNAME;
                             setPassword(username, password);
                           }
                           sec->close();
                         });
        QObject::connect(sec->ui->buttonBox, &QDialogButtonBox::rejected, this,
                         [this, sec]() -> void { sec->close(); });
        sec->show();
      });
}

#define ADD_SECURITY_ACTION                                                    \
  QAction *sec = new QAction();                                                \
  ui->menu_preferences->addAction(sec);                                        \
  modify_security_action(this, sec);
