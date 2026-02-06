#pragma once

#include "ui_change_password.h"
#include "ui_security.h"


#include <QMainWindow>

class SecurityForm;
class PasswordForm;

QT_BEGIN_NAMESPACE
namespace Ui {
    class SecurityForm;
    class PasswordForm;
}
QT_END_NAMESPACE



class SecurityForm : public QWidget {
    Q_OBJECT
public:
    Ui::SecurityForm *ui;
    explicit SecurityForm(QWidget *parent = nullptr);
};

class PasswordForm : public QDialog {
    Q_OBJECT
public:
    Ui::PasswordForm *ui;
    explicit PasswordForm(QWidget *parent = nullptr);
};
