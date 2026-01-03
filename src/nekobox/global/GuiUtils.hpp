#pragma once

#include "3rdparty/qv2ray/wrapper.hpp"
#include "3rdparty/QThreadCreateThread.hpp"
#include <nekobox/dataStore/Utils.hpp>
#include <functional>
#include <memory>
#include <QObject>
#include <QApplication>
#include <QTimer>
#include <QString>
#include <QDebug>
#include <QWidget>
#include <QThread>
#include <QFile>
#include <QMessageBox>
#include <QStyle>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

#ifdef Q_OS_WIN
#include "nekobox/sys/windows/guihelper.h"
#endif
// Dialogs

#define Dialog_DialogBasicSettings "DialogBasicSettings"
#define Dialog_DialogEditProfile "DialogEditProfile"
#define Dialog_DialogManageGroups "DialogManageGroups"
#define Dialog_DialogManageRoutes "DialogManageRoutes"
#define Dialog_DialogManageHotkeys "DialogManageHotkeys"

// Utils

#define QRegExpValidator_Number new QRegularExpressionValidator(QRegularExpression("^[0-9]+$"), this)

// Save&Load

#define P_C_LOAD_STRING(a) CACHE.a = bean->a;
#define P_C_SAVE_STRING(a) bean->a = CACHE.a;
#define D_C_LOAD_STRING(a) CACHE.a = Configs::dataStore->a;
#define D_C_SAVE_STRING(a) Configs::dataStore->a = CACHE.a;

#define P_LOAD_STRING(a) ui->a->setText(bean->a);
#define P_LOAD_STRING_PLAIN(a) ui->a->setPlainText(bean->a);
#define P_SAVE_STRING(a) bean->a = ui->a->text();
#define P_SAVE_STRING_PLAIN(a) bean->a = ui->a->toPlainText();

#define D_LOAD_STRING(a) ui->a->setText(Configs::dataStore->a);
#define D_LOAD_STRING_PLAIN(a) ui->a->setPlainText(Configs::dataStore->a);
#define D_SAVE_STRING(a) Configs::dataStore->a = ui->a->text();
#define D_SAVE_STRING_PLAIN(a) Configs::dataStore->a = ui->a->toPlainText();

#define P_LOAD_INT(a)                    \
    ui->a->setText(QString::number(bean->a)); \
    ui->a->setValidator(QRegExpValidator_Number);
#define P_SAVE_INT(a) bean->a = ui->a->text().toInt();

#define D_LOAD_INT(a)                                  \
    ui->a->setText(QString::number(Configs::dataStore->a)); \
    ui->a->setValidator(QRegExpValidator_Number);
#define D_SAVE_INT(a) Configs::dataStore->a = ui->a->text().toInt();

#define S_LOAD_INT(a, b)                                  \
    ui->a->setText(QString::number(settings.value(#a, b).toInt())); \
    ui->a->setValidator(QRegExpValidator_Number);
#define S_SAVE_INT(a) settings.setValue(#a, ui->a->text().toInt());

#define P_LOAD_COMBO_STRING(a) ui->a->setCurrentText(bean->a);
#define P_SAVE_COMBO_STRING(a) bean->a = ui->a->currentText();

#define D_LOAD_COMBO_STRING(a) ui->a->setCurrentText(Configs::dataStore->a);
#define D_SAVE_COMBO_STRING(a) Configs::dataStore->a = ui->a->currentText();

#define P_LOAD_COMBO_INT(a) ui->a->setCurrentIndex(bean->a);
#define P_SAVE_COMBO_INT(a) bean->a = ui->a->currentIndex();

#define D_LOAD_BOOL(a) ui->a->setChecked(Configs::dataStore->a);
#define S_LOAD_BOOL(a, b) ui->a->setChecked(settings.value(#a, b).toBool());
#define D_SAVE_BOOL(a) Configs::dataStore->a = ui->a->isChecked();
#define S_SAVE_BOOL(a) settings.setValue(#a, ui->a->isChecked());

#define P_LOAD_BOOL(a) ui->a->setChecked(bean->a);
#define P_SAVE_BOOL(a) bean->a = ui->a->isChecked();

#define EMPTY_JOB {}

#define CREATE_LINK(name) {                                                 \
    QString fileName = QFileDialog::getOpenFileName(this,                   \
        QObject::tr("Select"), Configs::resourceManager->getLatestPath(),   \
        "", nullptr, QFileDialog::Option::ReadOnly);                        \
    Configs::resourceManager->saveLink(name, fileName);                     \
}

#define LINK_RESOURCE_MANAGER(name, id, job) {                  \
    connect(ui->id, &QPushButton::clicked, this, [=, this](){   \
        CREATE_LINK(name)                                       \
        job  ;                                                  \
    });                                                         \
}

#define D_LOAD_INT_ENABLE(i, e)                             \
    if (Configs::dataStore->i > 0) {                        \
        ui->e->setChecked(true);                            \
        ui->i->setText(QString::number(Configs::dataStore->i));  \
    } else {                                                \
        ui->e->setChecked(false);                           \
        ui->i->setText(QString::number(-Configs::dataStore->i)); \
    }                                                       \
    ui->i->setValidator(QRegExpValidator_Number);
#define D_SAVE_INT_ENABLE(i, e)                         \
    if (ui->e->isChecked()) {                           \
        Configs::dataStore->i = ui->i->text().toInt();  \
    } else {                                            \
        Configs::dataStore->i = -ui->i->text().toInt(); \
    }

#define C_EDIT_JSON_ALLOW_EMPTY(a)                                    \
    auto editor = new JsonEditor(QString2QJsonObject(CACHE.a), this); \
    auto result = editor->OpenEditor();                               \
    CACHE.a = QJsonObject2QString(result, true);                      \
    if (result.isEmpty()) CACHE.a = "";                               \
    editor->deleteLater();

//

#define ADD_ASTERISK(parent)                                         \
    for (auto label: parent->findChildren<QLabel *>()) {             \
        auto text = label->text();                                   \
        if (!label->toolTip().isEmpty() && !text.endsWith("*")) {    \
            label->setText(text + "*");                              \
        }                                                            \
    }                                                                \
    for (auto checkBox: parent->findChildren<QCheckBox *>()) {       \
        auto text = checkBox->text();                                \
        if (!checkBox->toolTip().isEmpty() && !text.endsWith("*")) { \
            checkBox->setText(text + "*");                           \
        }                                                            \
    }


// UI
extern QWidget *mainwindow;

QWidget *GetMessageBoxParent();

#define MessageBoxWarning(T, X) \
    runOnUiThread([=, this](){                                      \
      QMessageBox::warning(this, T, X) ;                            \
})                                                                  \

#define MessageBoxInfo(T, X) \
    runOnUiThread([=, this](){                                      \
      QMessageBox::information(this, T, X) ;                        \
})                                                                  \

void ActivateWindow(QWidget *w);

void ToggleWindow(QWidget *w);

void runOnUiThread(const std::function<void()> &callback);


template<typename EMITTER, typename SIGNAL, typename RECEIVER, typename ReceiverFunc>
inline void connectOnce(EMITTER *emitter, SIGNAL signal, RECEIVER *receiver, ReceiverFunc f,
                        Qt::ConnectionType connectionType = Qt::AutoConnection) {
    auto connection = std::make_shared<QMetaObject::Connection>();
    auto onTriggered = [connection, f](auto... arguments) {
        std::invoke(f, arguments...);
        QObject::disconnect(*connection);
    };

    *connection = QObject::connect(emitter, signal, receiver, onTriggered, connectionType);
}

void setTimeout(const std::function<void()> &callback, QObject *obj, int timeout = 0);


inline bool isDarkMode() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return qApp->style()->standardPalette().window().color().lightness() < qApp->style()->standardPalette().windowText().color().lightness();
#endif
}
