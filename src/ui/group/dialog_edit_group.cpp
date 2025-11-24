#include "include/ui/group/dialog_edit_group.h"

#include "include/dataStore/Database.hpp"
#include "include/ui/mainwindow_interface.h"


#include <QClipboard>
#include <QStringListModel>
#include <QCompleter>

#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);

DialogEditGroup::DialogEditGroup(const std::shared_ptr<Configs::Group> &ent, QWidget *parent) : QDialog(parent), ui(new Ui::DialogEditGroup) {
    ui->setupUi(this);
    this->ent = ent;

    connect(ui->type, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        ui->cat_sub->setHidden(index == 0);
        ADJUST_SIZE
    });

    ui->name->setText(ent->name);
    ui->archive->setChecked(ent->archive);
    ui->skip_auto_update->setChecked(ent->skip_auto_update);
    ui->url->setText(ent->url);
    ui->type->setCurrentIndex(ent->url.isEmpty() ? 0 : 1);
    ui->type->currentIndexChanged(ui->type->currentIndex());
    ui->manually_column_width->setChecked(ent->manually_column_width);
    if (Configs::profileManager->GetProfile(ent->front_proxy_id) == nullptr) {
        ent->front_proxy_id = -1;
        ent->Save();
    }
    if (Configs::profileManager->GetProfile(ent->landing_proxy_id) == nullptr) {
        ent->landing_proxy_id = -1;
        ent->Save();
    }

    bool disable_share = true;

    if (ent->id >= 0) { // already a group
        ui->type->setDisabled(true);
        if (!ent->Profiles().isEmpty()) {
            disable_share = false;
        }
    }

/*
    ui->front_proxy->setEditable(true);
    ui->front_proxy->setInsertPolicy(QComboBox::NoInsert);
    ui->front_proxy->setCompleter(nullptr);

    ui->landing_proxy->setEditable(true);
    ui->landing_proxy->setInsertPolicy(QComboBox::NoInsert);
    ui->landing_proxy->setCompleter(nullptr);
*/
    connect(ui->copy_links, &QPushButton::clicked, this, [=,this] {
        QStringList links;
        for (const auto &[_, profile]: Configs::profileManager->profiles) {
            if (profile->gid != ent->id) continue;
            links += profile->bean->ToShareLink();
        }
        QApplication::clipboard()->setText(links.join("\n"));
        MessageBoxInfo(software_name, tr("Copied"));
    });
    connect(ui->copy_links_nkr, &QPushButton::clicked, this, [=,this] {
        QStringList links;
        for (const auto &[_, profile]: Configs::profileManager->profiles) {
            if (profile->gid != ent->id) continue;
            links += profile->bean->ToNekorayShareLink(profile->type);
        }
        QApplication::clipboard()->setText(links.join("\n"));
        MessageBoxInfo(software_name, tr("Copied"));
    });

    connect(ui->common_tabs, &QTabWidget::currentChanged, this, [=,this] (int index) {
        if (index == 1){
            if (CACHE.proxy_items_need_refresh){
                qDebug() <<" Refresh proxy list for group editor ";
                CACHE.proxy_items_need_refresh = false;
                on_refresh_proxy_items();
            }
        }
    });

    ui->name->setFocus();
    ADJUST_SIZE

    if (disable_share){
        ui->common_tabs->removeTab(2);
    }
}

DialogGroupChooseProxy::DialogGroupChooseProxy(QWidget * parent): QDialog(parent), ui(new Ui::DialogGroupChooseProxy) {
    ui->setupUi(this);
}

DialogGroupChooseProxy::~DialogGroupChooseProxy(){
    delete ui;
}

DialogEditGroup::~DialogEditGroup() {
    delete ui;
}

void DialogEditGroup::accept() {
    if (ent->id >= 0) { // already a group
        if (!ent->url.isEmpty() && ui->url->text().isEmpty()) {
            MessageBoxWarning(tr("Warning"), tr("Please input URL"));
            return;
        }
    }
    ent->name = ui->name->text();
    ent->url = ui->url->text();
    ent->archive = ui->archive->isChecked();
    ent->skip_auto_update = ui->skip_auto_update->isChecked();
    ent->manually_column_width = ui->manually_column_width->isChecked();
    if (!CACHE.proxy_items_need_refresh){
        if (CACHE.proxy_landing_changed){
  //          QString front_proxy_text = ui->front_proxy->currentText();
  //          ent->front_proxy_id = get_proxy_id(front_proxy_text);
        }
        if (CACHE.proxy_front_changed){
  //          QString landing_proxy_text = ui->landing_proxy->currentText();
  //          ent->landing_proxy_id = get_proxy_id(landing_proxy_text);
        }
    }
    QDialog::accept();
}

void DialogEditGroup::on_refresh_proxy_items(){
    /*
    QStringList proxy_items_list;
    for (const auto &item: Configs::profileManager->profiles) {
        QString name = (item.second->bean->DisplayName());
        int id = item.first;
        if (name.isEmpty()){
            name = QString::number(id);
        }
        proxy_items_list.push_back(name);
        proxy_items[name] = id;
    }
    proxy_items[""] = -1;

    ui->front_proxy->addItems(proxy_items_list);
    ui->landing_proxy->addItems(proxy_items_list);

    QString front_name;
    QString landing_name;
*/
    ui->front_proxy->setText (get_proxy_name(ent->front_proxy_id).trimmed());
    ui->landing_proxy->setText(get_proxy_name(ent->landing_proxy_id).trimmed());

    connect(ui->front_proxy, &QCommandLinkButton::clicked, this, 
        [=, this](bool b) {
            auto window = new DialogGroupChooseProxy(this);
            window->show();
    });

    connect(ui->landing_proxy, &QCommandLinkButton::clicked, this, 
        [=, this](bool b) {
            auto window = new DialogGroupChooseProxy(this);
            window->show();
    });
/*
    auto Completer = new QCompleter(proxy_items_list, this);
    Completer->setCompletionMode(QCompleter::PopupCompletion);
    Completer->setCaseSensitivity(Qt::CaseInsensitive);
    Completer->setFilterMode(Qt::MatchContains);
    ui->front_proxy->lineEdit()->setCompleter(Completer);
    ui->landing_proxy->lineEdit()->setCompleter(Completer);
    */
}

QString DialogEditGroup::get_proxy_name(int id) {
    auto profiles = Configs::profileManager->profiles;
    if (!profiles.contains(id)){
        return "None";
    } else {
        QString str = profiles[id]->bean->DisplayName();
        if (str.isEmpty()){
            return "Id: "+ QString::number(id);
        } else {
            return str;
        }
    }
}
