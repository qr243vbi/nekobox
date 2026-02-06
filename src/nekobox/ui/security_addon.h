#pragma once
#include <QByteArray>
#include <QString>
#include <QFile>
#include <QSettings>
#include "nekobox/ui/security/security.h"

#define KEYS_INI_PATH QDir::current().absolutePath() + "/keys.ini"

extern QSettings * local_keys;
extern QSettings * global_keys;

QByteArray EncryptData(const QByteArray & value, const QByteArray & keys);
QByteArray DecryptData(const QByteArray & value, const QByteArray & keys);

QByteArray GetSoftwareKeys();

QByteArray ResetEncryptionKeys();

void SaveBackup(QFile file);

QByteArray GetEncryptionKeys();

QByteArray GetPassword();

void SetPassword(const QByteArray &array);

QByteArray DecryptData(const QByteArray &array);

QByteArray EncryptData(const QByteArray &array);

inline void SaveBackup(const QString &path){
    SaveBackup(QFile(path));
};