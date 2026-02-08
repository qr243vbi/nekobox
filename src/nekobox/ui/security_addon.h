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

bool getLocked(LockValue key, const QString & username = "");

void setLocked(LockValue key, bool value, const QString & username = "");

QByteArray getPasswordHash(const QString &username);

bool checkPassword(const QByteArray& hash);

void setPassword(const QString &username, const QString& password);

void setInboundPassword(const QString &username, const QString& password);

void addUser(const QString & username);
void delUser(const QString & username);