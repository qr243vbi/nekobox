#pragma once
#include <QByteArray>
#include <QString>
#include <QFile>
#include <QSettings>
#include "nekobox/ui/security/security.h"

#define KEYS_INI_PATH QDir::current().absolutePath() + "/keys.ini"

extern QSettings * local_keys;

enum LockValue{
    LockStartup,
    LockSettings,
    LockSystray 
};

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