#ifndef INFO_DIALOG_H
#define INFO_DIALOG_H
#include "ui_main.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
    class InfoMain;
}
QT_END_NAMESPACE


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