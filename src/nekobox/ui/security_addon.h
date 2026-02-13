#pragma once
#include <QByteArray>
#include <QString>
#include <QFile>
#include <QSettings>
#include <QAction>
#include "nekobox/ui/security/security.h"


#ifndef NKR_DEFAULT_PASSWORD
#define NKR_DEFAULT_PASSWORD NKR_SOFTWARE_KEYS
#endif

#ifndef NKR_DEFAULT_USERNAME
#define NKR_DEFAULT_USERNAME "admin"
#endif

void set_access_denied(QWidget * w);


#define CHECK_ACCESS_W(X) \
  if (!confirmLock(LockValue::Lock##X)) { \
    QWidget * wd = new QDialog(); \
    set_access_denied(wd); \
    wd->show(); \
    return; \
  }

#define CHECK_ACCESS(X) \
  if (!confirmLock(LockValue::Lock##X)) { \
    set_access_denied(this); \
    return; \
  }

#define CHECK_SETTINGS_ACCESS_W CHECK_ACCESS_W(Settings)
#define CHECK_SETTINGS_ACCESS CHECK_ACCESS(Settings)

class MainWindow;

void modify_security_action(MainWindow *win, QAction *sec) ;

#define ADD_SECURITY_ACTION                                                    \
  QAction *sec = new QAction();                                                \
  ui->menu_preferences->addAction(sec);                                        \
  modify_security_action(this, sec);


#define KEYS_INI_PATH QDir::current().absolutePath() + "/keys.ini"

extern QSettings * local_keys;

enum LockValue{
    LockStartup,
    LockSettings,
    LockSystray 
};


#define CHECK_ACTION_ACCESS_W CHECK_ACCESS_W(Startup)
#define CHECK_ACTION_ACCESS CHECK_ACCESS(Startup)

extern long long time_startup;
extern long long time_settings;
extern long long time_systray;

bool confirmLock(LockValue val);

bool getLocked(LockValue key, const QString & username = "");

void setLocked(LockValue key, bool value, const QString & username = "");

QByteArray getPasswordHash(const QString &username);

bool checkPassword(const QString &username, const QString &password);

void setPassword(const QString &username, const QString& password);

void setInboundPassword(const QString &username, const QString& password);

void addUser(const QString & username);
void delUser(const QString & username);

void init_keys();

#include "nekobox/ui/mainwindow.h"