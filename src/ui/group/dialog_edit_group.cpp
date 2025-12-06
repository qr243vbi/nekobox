#include "include/ui/group/dialog_edit_group.h"

#include "include/dataStore/Database.hpp"
#include "include/ui/mainwindow_interface.h"
#include "include/sys/Settings.h"
#include <QHeaderView>
#include <QClipboard>
#include <QStringListModel>
#include <QCompleter>
#include <QTableView>
#include <qdialog.h>
#include <qdialogbuttonbox.h>
#include <qnamespace.h>

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
    ui->manually_column_width->setChecked(Configs::tableSettings.manually_column_width);

    bool disable_share = true;

    if (ent->id >= 0) { // already a group
        ui->type->setDisabled(true);
        if (!ent->Profiles().isEmpty()) {
            disable_share = false;
        }
    }

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

    ui->name->setFocus();
    CACHE.landing_proxy_id = ent->landing_proxy_id;
    CACHE.front_proxy_id = ent->front_proxy_id;
    CACHE.edited = false;
    CACHE.loaded = false;

    connect(ui->tabs, &QTabWidget::currentChanged, 
        this, &DialogEditGroup::on_refresh_proxy_items);

    if (disable_share){
        ui->tabs->tabBar()->setTabVisible(2, false);
    }
    
    ADJUST_SIZE

}

#include <QSortFilterProxyModel>

class GroupTableProxyModel : public QSortFilterProxyModel {

public:
    GroupTableProxyModel(DialogGroupChooseProxy *parent = nullptr) : QSortFilterProxyModel(parent) {
        this->parent = parent;
    }
    DialogGroupChooseProxy * parent;
protected:
    // Override this function to implement custom filtering logic
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
        // Get the model index for the 2nd and 3rd columns
        QModelIndex index2 = sourceModel()->index(sourceRow, 1, sourceParent); // 2nd column (index 1)
        QModelIndex index3 = sourceModel()->index(sourceRow, 2, sourceParent); // 3rd column (index 2)
        
        // Get the data from those columns
        QString data2 = index2.data().toString();
        QString data3 = index3.data().toString();
        
        // Define your filter criteria. Here it's an example check if values contain "filterText"
        QString filterText = parent->ui->search_line->text();
        bool matchesColumn2 = data2.contains(filterText, Qt::CaseInsensitive);
        bool matchesColumn3 = data3.contains(filterText, Qt::CaseInsensitive);

        // Accept the row if it matches either column
        return matchesColumn2 || matchesColumn3;
    }
};

class GroupTableModel : public QAbstractTableModel {
public:
    std::shared_ptr<Configs::Group> group;
    GroupTableModel(std::shared_ptr<Configs::Group> data, QObject *parent = nullptr)
        : QAbstractTableModel(parent), group(data) {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        // Return the number of rows
        return group->profiles.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        // Return the number of columns
        return 3;  // For a single-column model
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            if (orientation == Qt::Horizontal) {
                switch (section) {
                    case 0:
                        return QCoreApplication::translate(
        "MainWindow","Type");
                    case 1:
                        return QCoreApplication::translate(
        "MainWindow","Address");
                    case 2:
                        return QCoreApplication::translate(
        "MainWindow","Name");
                }
            } else if (orientation == Qt::Vertical) {
                return QString::number(group->profiles[section]);
            }
        }
        return QVariant();
    }

#define DO_NOTHING_ROLE 909090
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {

        if (role == DO_NOTHING_ROLE){
            return group->profiles.at(index.row());
        }
        if (role == Qt::DisplayRole) {
            int row = index.row();
            auto & profiles = group->profiles;
            if (profiles.size() <= row){
                return QVariant();
            }
            int column = index.column();
            if (column < 3 && column >= 0){
                int profile_id = profiles.at(row);
                auto & bean = 
                    Configs::profileManager->profiles.at(profile_id)->bean;
                switch(column){
                    case 0:
                    return bean->DisplayType();
                    case 1:
                    return bean->DisplayAddress();
                    case 2:
                    return bean->name;
                }
            }
        }
        return QVariant();
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
        return false;
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override {
        if (!index.isValid())
            return Qt::ItemFlag::NoItemFlags;
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }

private:
    QStringList itemList;
};

DialogGroupChooseProxy::DialogGroupChooseProxy(QWidget * parent): QDialog(parent), ui(new Ui::DialogGroupChooseProxy) {
    ui->setupUi(this);
    auto groups_widget = ui->groups;
    groups_widget->clear();
    groups.clear();

    auto current = Configs::profileManager->CurrentGroup();
    int curid = current->id;
    int tabid = 0;
    QWidget * current_tab;
    std::shared_ptr<Configs::Group> current_group;
    for (auto i : Configs::profileManager->groups){
        int id = i.first;
        QWidget * widget = new QWidget();
        auto cur_group = i.second;
        if (id == curid){
            current_tab = widget;
            tabid = groups.size();
            current_group = cur_group;
        }
        groups.push_back(id);
        groups_widget->addTab(widget, cur_group->name);
    }
    connect(groups_widget, &QTabWidget::currentChanged, 
        this, &DialogGroupChooseProxy::change_tab);
    auto buttons = ui->buttons;
    
    connect(buttons, &QDialogButtonBox::clicked, this, 
                &DialogGroupChooseProxy::dialog_button);

    if (tabid == 0){
        this->change_tab(tabid);
    } else {
        groups_widget->setCurrentIndex(tabid);
    }

}

void DialogGroupChooseProxy::dialog_button(QAbstractButton * button){
    auto role = this->ui->buttons->buttonRole(button);
    int id = -1;
    switch(role){
        case QDialogButtonBox::ResetRole:
            id = (this->default_id);
        case QDialogButtonBox::DestructiveRole:
            this->profile_selected(id);
        case QDialogButtonBox::ApplyRole:
        case QDialogButtonBox::AcceptRole:
            emit set_proxy(this->selected_id);
        case QDialogButtonBox::RejectRole:
        default:
            break;
    }
    switch(role){
        case QDialogButtonBox::AcceptRole:
        case QDialogButtonBox::RejectRole:
        case QDialogButtonBox::DestructiveRole:
        this->close();
        default:
        break;
    }
}

void DialogGroupChooseProxy::profile_selected(int profile, bool def){
    this->selected_id = profile;
    this->ui->profile_label->setText(DialogEditGroup::get_proxy_name(profile));
    if (def){
        this->default_id = profile;
    }
}

void DialogGroupChooseProxy::change_tab(int group){
    {
        auto current_tab = this->ui->groups->widget(group);
        auto layout = current_tab->layout();
        if (layout == nullptr){
            auto current_group = Configs::profileManager->groups[
                this->groups.at(group)];
            QVBoxLayout *mainLayout = new QVBoxLayout();
            GroupTableModel * model = new GroupTableModel(current_group, this);
            QTableView *tableView = new QTableView();
            mainLayout->addWidget(tableView);
            current_tab->setLayout(mainLayout);
            tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
            tableView->setSelectionMode(QAbstractItemView::SingleSelection);
            auto sel_model = tableView->selectionModel();

            QSortFilterProxyModel *proxyModel = new GroupTableProxyModel(this);
            proxyModel->setSourceModel(model);

            connect(tableView->selectionModel(), &QItemSelectionModel::currentChanged, 
                this, [sel_model, proxyModel, this](const QModelIndex &current, 
                    const QModelIndex &previous)->void{
                if (current.isValid()){
                    if (sel_model->hasSelection()){
                        int proid = proxyModel->data(current, DO_NOTHING_ROLE).toInt();
                        this->profile_selected(proid);
                    }
                }
            });
            connect(tableView, &QTableView::clicked, 
                this, [proxyModel, this](const QModelIndex &current)->void{
                if (current.isValid()){
                    int proid = proxyModel->data(current, DO_NOTHING_ROLE).toInt();
                    this->profile_selected(proid);
                }
            });
            connect(tableView, &QTableView::selectRow, 
                this, [proxyModel, this](int row)->void{
                QModelIndex current = proxyModel->index(row, 1);
                int proid = proxyModel->data(current, DO_NOTHING_ROLE).toInt();
                this->profile_selected(proid);
            });
            auto header = tableView->horizontalHeader();
            header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
            ui->profile_label->setReadOnly(true);

        // Connect the search edit to filter the proxy model
            connect(ui->search_line, &QLineEdit::textChanged, proxyModel, 
                &QSortFilterProxyModel::setFilterFixedString);
            tableView->setModel(proxyModel);

        } else {
            QTableView * view = (QTableView*)(layout->itemAt(0)->widget());
            view->clearSelection();
        }
    }
}

DialogGroupChooseProxy::~DialogGroupChooseProxy(){
    delete ui;
}

void DialogEditGroup::set_landing_proxy(int id){
    CACHE.landing_proxy_id = id;
    CACHE.edited = true;
    ui->landing_proxy->setText(get_proxy_name(id).trimmed());
}

void DialogEditGroup::set_front_proxy(int id){
    CACHE.front_proxy_id = id;
    CACHE.edited = true;
    ui->front_proxy->setText(get_proxy_name(id).trimmed());
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
    Configs::tableSettings.manually_column_width = ui->manually_column_width->isChecked();
    if (CACHE.edited){
        ent->landing_proxy_id = CACHE.landing_proxy_id;
        ent->front_proxy_id = CACHE.front_proxy_id;
    }
    if (NOTES.edited){
        QString notes = ui->group_notes->toPlainText();
        ent->saveNotes(notes);
    }
    ent->Save();

    QDialog::accept();
}

void DialogEditGroup::on_refresh_proxy_items(int index){
{
        if (index == 1){
            if (!CACHE.loaded){
                CACHE.loaded = true;
                goto on_refresh_proxy_items_1;
            }
        } else if (index == 3){
            if (!NOTES.loaded){
                NOTES.loaded = true;
                ui->group_notes->setText(this->ent->getNotes());
                connect(ui->group_notes, &QTextEdit::textChanged, 
                    this, [this](){
                        NOTES.edited = true;
                }, Qt::SingleShotConnection);
            }
        }
        if (NOTES.loaded && CACHE.loaded){
            disconnect(ui->tabs, &QTabWidget::currentChanged, this, &DialogEditGroup::on_refresh_proxy_items);
        }
    }

    return;

    on_refresh_proxy_items_1:

    ui->front_proxy->setText (get_proxy_name(ent->front_proxy_id).trimmed());
    ui->landing_proxy->setText(get_proxy_name(ent->landing_proxy_id).trimmed());

    connect(ui->front_proxy, &QCommandLinkButton::clicked, this, 
        [=, this](bool b) {
            auto window = new DialogGroupChooseProxy(this);
            QObject::connect(window, &DialogGroupChooseProxy::set_proxy, 
                this, &DialogEditGroup::set_front_proxy);
            window->setWindowTitle(QCoreApplication::translate(
        "DialogGroupChooseProxy","Front proxy for group %1").arg(ent->name));
            window->ui->proxy_label->setText(QCoreApplication::translate(
        "DialogGroupChooseProxy","Front proxy: "));
            window->profile_selected(ent->front_proxy_id, true);
            window->show();
    });

    connect(ui->landing_proxy, &QCommandLinkButton::clicked, this, 
        [=, this](bool b) {
            auto window = new DialogGroupChooseProxy(this);
            QObject::connect(window, &DialogGroupChooseProxy::set_proxy, 
                this, &DialogEditGroup::set_landing_proxy);
            window->setWindowTitle(QCoreApplication::translate(
        "DialogGroupChooseProxy","Landing proxy for group %1").arg(ent->name));
            window->ui->proxy_label->setText(QCoreApplication::translate(
        "DialogGroupChooseProxy","Landing proxy: "));
            window->profile_selected(ent->landing_proxy_id, true);
            window->show();
    });
}

QString DialogEditGroup::get_proxy_name(int id) {
    std::shared_ptr<Configs::ProxyEntity> profile;
    if (id < 0 ||
        (profile = Configs::profileManager->GetProfile(id)) == nullptr){
        return QCoreApplication::translate(
        "DialogGroupChooseProxy", "None");
    } else {
        QString str = profile->bean->name;
        if (str.isEmpty()){
            return QString("ID: ")+ QString::number(id);
        } else {
            return str;
        }
    }
}
