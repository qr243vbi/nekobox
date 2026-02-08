#pragma once

#include "ui_change_password.h"
#include "ui_confirm_password.h"
#include "ui_security.h"
#include "ui_delete_or_edit_users.h"

#include <QMainWindow>

class SecurityForm;
class PasswordForm;

QT_BEGIN_NAMESPACE
namespace Ui {
    class SecurityForm;
    class PasswordForm;
    class ConfirmForm;
    class UsersForm;
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

class ConfirmForm : public QDialog {
    Q_OBJECT
public:
    Ui::ConfirmForm *ui;
    explicit ConfirmForm(QWidget *parent = nullptr);
};


class UsersForm : public QDialog {
    Q_OBJECT
public:
    Ui::UsersForm *ui;
    explicit UsersForm(QWidget *parent = nullptr);
};

