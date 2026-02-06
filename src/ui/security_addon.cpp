#pragma once
#include <QAction>
#include "nekobox/ui/mainwindow.h"
#include "nekobox/ui/security/security.h"
#include "ui_change_password.h"
#include "ui_security.h"
#include "nekobox/ui/security_addon.h"
#include "nekobox/dataStore/Utils.hpp"
#include <QtTranslation>
#include <qlineedit.h>
#include <QSettings>
#include <QCryptographicHash>


QSettings * local_keys  = nullptr;
QSettings * global_keys = nullptr;

void init_keys(){
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    // Set the path: organization and application
    local_keys = new QSettings(KEYS_INI_PATH, QSettings::IniFormat);
    QString value = serverName + "_" + GetRandomString(16);
    
    global_keys = new QSettings("qr243vbi", "NyameBox");
//    global_keys.setPath(QSettings::NativeFormat, QSettings::UserScope, )

}


QByteArray EncryptData(const QByteArray & value, const QByteArray & keys){
    
};

QByteArray DecryptData(const QByteArray & value, const QByteArray & keys){

};

QByteArray GetSoftwareKeys(){
    static bool keys_generated = false;
    static QByteArray software_keys;
    if (!keys_generated){
        keys_generated = true;
        software_keys = QCryptographicHash::hash(NKR_SOFTWARE_KEYS, QCryptographicHash::Sha256);
    }
    return software_keys;
};

#ifndef NKR_DEFAULT_PASSWORD
#define NKR_DEFAULT_PASSWORD "0000"
#endif

QByteArray ResetEncryptionKeys(){};

void SaveBackup(QFile file){};

QByteArray GetEncryptionKeys(){};

QByteArray GetPassword(){
//    QByteArray arr = settings.value("hash", ).toByteArray();
};

void SetPassword(const QByteArray &array){

};

static inline void modify_security_action(MainWindow * win, QAction * sec){
    sec->setText(QAction::tr("Security Settings"));
    
    QObject::connect(
        sec, &QAction::triggered, win,
        [win]() {
            auto sec = new SecurityForm(win);
            sec->show();
    });
}

SecurityForm::SecurityForm(QWidget * parent){
    ui = new Ui::SecurityForm();
    ui->setupUi(this);
    QObject::connect(ui->change_ui_password, &QPushButton::clicked, this, [this](){
        auto sec = new PasswordForm(this);
        QObject::connect(sec->ui->buttonBox, &QDialogButtonBox::accepted, this, [sec, this]()->void{
            sec->close();
        });
        QObject::connect(sec->ui->buttonBox, &QDialogButtonBox::rejected, this, [this, sec]()->void{
            sec->close();
        });
        sec->show();
    });

    QObject::connect(ui->change_proxy_password, &QPushButton::clicked, this, [this](){
        auto sec = new PasswordForm(this);
        sec->ui->password_label->setText(QObject::tr("Username"));
        sec->ui->curpass->setEchoMode(QLineEdit::Normal);
        QObject::connect(sec->ui->buttonBox, &QDialogButtonBox::accepted, this, [sec, this]()->void{
            sec->close();
        });
        QObject::connect(sec->ui->buttonBox, &QDialogButtonBox::rejected, this, [this, sec]()->void{
            sec->close();
        });
        sec->show();
    });
}

PasswordForm::PasswordForm(QWidget * parent){
    ui = new Ui::PasswordForm();
    ui->setupUi(this);
}

#define ADD_SECURITY_ACTION QAction * sec = new QAction(); ui->menu_preferences->addAction(sec); modify_security_action(this, sec);
