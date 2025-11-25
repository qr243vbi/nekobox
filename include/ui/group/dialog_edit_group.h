#pragma once

#include <QDialog>
#include <QMap>
#include <QString>
#include <QEvent>
#include <qtmetamacros.h>
#include "include/dataStore/Group.hpp"
#include <vector>
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

   static QString get_proxy_name(int id);
private:
    Ui::DialogEditGroup *ui;

    std::shared_ptr<Configs::Group> ent;

    void on_refresh_proxy_items();
private slots:
    void accept() override;
    void set_landing_proxy(int id);
    void set_front_proxy(int id);
};


class DialogGroupChooseProxy: public QDialog {
    Q_OBJECT
private:
    std::vector<int> groups;
    int selected_id = -1;
    int default_id = -1;
public:
    Ui::DialogGroupChooseProxy *ui;
    explicit DialogGroupChooseProxy(QWidget *parent = nullptr);

    ~DialogGroupChooseProxy() override;
signals: 
    void set_proxy(int id);

public slots:
    void change_tab(int id);
    void profile_selected(int id, bool def = false);

    void dialog_button(QAbstractButton *button);
};