#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include <QLineEdit>

class MyLineEdit : public QLineEdit {
public slots:

    explicit MyLineEdit(QWidget *parent = nullptr) : QLineEdit(parent) {
    }

    void setText(const QString &s) {
        QLineEdit::setText(s);
        QLineEdit::home(false);
    }
};
