#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#ifndef INFO_DIALOG_H
#define INFO_DIALOG_H
#include "ui_main.h"
#include "ui_about.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
    class InfoMain;
    class AboutMain;
}
QT_END_NAMESPACE

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);

    ~AboutDialog() override;

private:
    Ui::AboutMain *ui;

public slots:

    void accept() override;
};

class InfoDialog : public QDialog {
    Q_OBJECT

public:
    explicit InfoDialog(QWidget *parent = nullptr);

    ~InfoDialog() override;

private:
    Ui::InfoMain *ui;

public slots:

    void accept() override;
};

#endif