#pragma once

#include <QDialog>
#include <QMap>
#include <QString>
#include <QEvent>
#include <qtmetamacros.h>
#include "include/dataStore/Group.hpp"
#include "ui_dialog_edit_group.h"

#include "ui_dialog_group_choose_proxy.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEditGroup;
    class DialogGroupChooseProxy;
}
QT_END_NAMESPACE


class DialogEditGroup : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditGroup(const std::shared_ptr<Configs::Group> &ent, QWidget *parent = nullptr);

    ~DialogEditGroup() override;

private:
    Ui::DialogEditGroup *ui;

    std::shared_ptr<Configs::Group> ent;

    struct  {
        bool proxy_items_need_refresh = true;
        bool proxy_landing_changed = false;
        bool proxy_front_changed = false;
    } CACHE;

private slots:

    void accept() override;

    QString get_proxy_name(int id);

    void on_refresh_proxy_items();
};


class DialogGroupChooseProxy: public QDialog {
    Q_OBJECT
    public:

    explicit DialogGroupChooseProxy(QWidget *parent = nullptr);

    ~DialogGroupChooseProxy() override;
    private:
    Ui::DialogGroupChooseProxy *ui;
};