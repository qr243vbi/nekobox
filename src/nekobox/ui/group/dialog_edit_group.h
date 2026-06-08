



#pragma once

#include <QDialog>
#include <QMap>
#include <QString>
#include <QEvent>
#include <qtmetamacros.h>
#include <nekobox/ui/setting/RouteItem.h>
#include <nekobox/dataStore/Group.hpp>
#include <vector>
#include "ui_dialog_edit_group.h"
#include "ui_dialog_edit_subscription.h"
#include "ui_dialog_edit_subscription_hwid.h"
#include "ui_dialog_edit_subscription_headers.h"
#include "ui_dialog_group_choose_proxy.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEditGroup;
    class DialogEditSubscription;
    class DialogGroupChooseProxy;
    class DialogHWID;
    class DialogHeaders;
}
QT_END_NAMESPACE

class DialogHWID: public QDialog {
    Q_OBJECT
public:
    explicit DialogHWID(QWidget * parent = nullptr);
    ~DialogHWID() override;
    Ui::DialogHWID *ui;
};

class DialogHeaders: public QDialog {
    Q_OBJECT
public:
    explicit DialogHeaders(QWidget * parent = nullptr);
    void addRow(const QString & title, const QString & value);
    ~DialogHeaders() override;
    Ui::DialogHeaders *ui;
};

class DialogEditSubscription: public QDialog {
    Q_OBJECT
public:
    explicit DialogEditSubscription(
        std::shared_ptr<Configs::Group> ent,
        QWidget *parent = nullptr);

    ~DialogEditSubscription() override;

    bool edited = false;
    void save();
private:
    Ui::DialogEditSubscription *ui;
    std::shared_ptr<Configs::Group> ent;

};

class DialogEditGroup : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditGroup(const std::shared_ptr<Configs::Group> &ent, QWidget *parent = nullptr);

    ~DialogEditGroup() override;

   static QString get_proxy_name(int id, bool is_for_routeprofile =  false);
private:
    Ui::DialogEditGroup *ui;
    struct {
        bool edited ;
        bool loaded ;
        int landing_proxy_id ;
        int front_proxy_id ;
    } CACHE;
    struct {
        bool edited;
        bool loaded;
    } NOTES;

    std::shared_ptr<Configs::Group> ent;

    void on_refresh_proxy_items(int id);
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
    int last_id = -1;
    bool is_for_routeprofile = false;

public:
    friend class RouteItem;
    Ui::DialogGroupChooseProxy *ui;
    explicit DialogGroupChooseProxy(QWidget *parent = nullptr);

    ~DialogGroupChooseProxy() override;
signals: 
    void set_proxy(int id);
    void select_proxy(int id);

public slots:
    void change_tab(int id);
    void profile_selected(int id, bool def = false);

    void dialog_button(QAbstractButton *button);
};
