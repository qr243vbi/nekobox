#include <nekobox/ui/group/dialog_edit_group.h>
#include <QSortFilterProxyModel>
#include <nekobox/dataStore/Database.hpp>
#include <nekobox/ui/mainwindow_interface.h>
#include <nekobox/sys/Settings.h>
#include <QHeaderView>
#include <nekobox/global/GuiUtils.hpp>
#include <QClipboard>
#include <QStringListModel>
#include <QCompleter>
#include <QTableView>
#include <qdialog.h>
#include <functional>
#include <qdialogbuttonbox.h>
#include <qnamespace.h>
#include <qtablewidget.h>

#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);

DialogHWID::DialogHWID(QWidget * parent ) : QDialog(parent), ui(new Ui::DialogHWID()) {
    ui->setupUi(this);
}
DialogHWID::~DialogHWID(){
    delete ui;
}
DialogHeaders::DialogHeaders(QWidget * parent ) : QDialog(parent), ui(new Ui::DialogHeaders()) {
    ui->setupUi(this);
    QTableWidget * tableWidget = ui->custom_headers_table;
    QHeaderView *header = tableWidget->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    auto button = ui->buttonBox->addButton(tr("New"), QDialogButtonBox::ButtonRole::ActionRole);
    ui->buttonBox->setStandardButtons(QDialogButtonBox::Discard | QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

    QPushButton *discardButton = ui->buttonBox->button(QDialogButtonBox::Discard);

    tableWidget->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );

    tableWidget->setSelectionMode(
        QAbstractItemView::ExtendedSelection
    );

    connect(discardButton, &QPushButton::clicked, this, [this](){
        
    });

    connect(button, &QPushButton::clicked, this, [this](){
        bool ok;
        QString text = QInputDialog::getText(
        this,
        software_name,
        tr("Header name"),
        QLineEdit::Normal,
        "",
        &ok
        );
        if (ok){
            QTableWidget * tableWidget = ui->custom_headers_table;
            int row = tableWidget->rowCount();
            tableWidget->insertRow(row);

            // Column 0: read-only
            auto *item0 = new QTableWidgetItem(text);
            item0->setFlags(item0->flags() & ~Qt::ItemIsEditable);
            tableWidget->setItem(row, 0, item0);

            // Column 1: editable
            auto *item1 = new QTableWidgetItem("");
            item1->setFlags(item1->flags() | Qt::ItemIsEditable);
            tableWidget->setItem(row, 1, item1);
        }
    });
}
DialogHeaders::~DialogHeaders(){
    delete ui;
}

DialogEditGroup::DialogEditGroup(const std::shared_ptr<Configs::Group> &ent, QWidget *parent) : QDialog(parent), ui(new Ui::DialogEditGroup) {
    CHECK_SETTINGS_ACCESS
    ui->setupUi(this);
    this->ent = ent;

    connect(ui->type, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        bool is_basic = index == 0;
        ui->auto_update->setHidden(is_basic);
        ui->url->setHidden(is_basic);
        ui->label_url->setHidden(is_basic);
        ui->sub_extra->setHidden(is_basic);
        ent->is_subscription = !is_basic;
        ui->name->setPlaceholderText(
            is_basic ? "" : 
            tr("Will use profile-title from subscription if empty")
        );
        ADJUST_SIZE
    });

    connect(ui->sub_extra, &QPushButton::clicked, this, [this](){
        auto window = new DialogEditSubscription(this->ent, this);
        window->show();
        window->exec();
        if (window->edited){
            window->save();
        }
        delete window;
    });
    
    ui->name->setText(ent->name);
    ui->archive->setChecked(ent->archive);
    if (ent->is_subscription){
        std::shared_ptr<Configs::GroupExtra> extra = ent->getExtra();
        ui->auto_update->setChecked(!extra->skip_auto_update);
        ui->url->setText(extra->url);
        ui->type->setCurrentIndex(1);
    } else {
        ui->type->setCurrentIndex(0);
    }
    ui->type->currentIndexChanged(ui->type->currentIndex());

    bool disable_share = true;

    if (ent->id >= 0) { // already a group
        ui->type->setHidden(true);
        ui->label_2->setHidden(true);
        if (!ent->Profiles().isEmpty()) {
            disable_share = false;
        }
    }

    std::function<void(bool)> copy_click = [group = ent, this] (bool neko){
        QStringList links;

        for (auto profileid: group->profiles) {
            auto profile = Configs::profileManager->GetProfile(profileid);
            if (profile == nullptr){
                continue;
            }
            auto bean = profile->bean();
            if (bean == nullptr){
                continue;
            }
            if (neko){
                links += bean->ToNekorayShareLink();
            } else {
                links += bean->ToShareLink();
            }
        }
        QApplication::clipboard()->setText(links.join("\n"));
        runOnUiThread([this](){
            QMessageBox::information(this, software_name, tr("Copied"));
        });
    };

    connect(ui->copy_links, &QPushButton::clicked, this, [copy_click](){
        copy_click(false);
    });

    connect(ui->copy_links_nkr, &QPushButton::clicked, this, [copy_click](){
        copy_click(true);
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

void DialogEditSubscription::save(){
    auto extra = this->ent->getExtra();
    extra->text_payload = this->ui->text->toPlainText();
    extra->javascript_payload = this->ui->javascript->toPlainText();
    extra->enable_custom_payload = this->ui->payload->isChecked();
    extra->enable_custom_headers = this->ui->custom_headers->isChecked();
    extra->Save();
}

DialogEditSubscription::DialogEditSubscription(
        std::shared_ptr<Configs::Group> ent,
        QWidget * widget): ui(new Ui::DialogEditSubscription), QDialog(widget), ent(ent) {
    ui->setupUi(this);
    auto extra = ent->getExtra();
    ui->custom_headers->setChecked(extra->enable_custom_headers);
    ui->payload->setChecked(extra->enable_custom_payload);
    ui->text->setPlainText(extra->text_payload);
    ui->javascript->setPlainText(extra->javascript_payload);

    QObject::connect(ui->hwid, &QPushButton::clicked, this, [this](bool clicked)->void{
        auto * hwid = new DialogHWID(this);
        hwid->show();
        hwid->exec();
        delete hwid;
    });


    QObject::connect(ui->headers, &QPushButton::clicked, this, [this](bool clicked)->void{
        auto * hwid = new DialogHeaders(this);
        hwid->show();
        hwid->exec();
        delete hwid;
    });

    QObject::connect(ui->custom_headers, &QGroupBox::toggled, this, [this](bool checked){
        edited = true;
    }, Qt::SingleShotConnection);

    QObject::connect(ui->payload, &QGroupBox::toggled, this, [this](bool checked){
        edited = true;
    }, Qt::SingleShotConnection);


    QObject::connect(ui->text, &QTextEdit::textChanged, this, [this](){
        edited = true;
    }, Qt::SingleShotConnection);


    QObject::connect(ui->javascript, &QTextEdit::textChanged, this, [this](){
        edited = true;
    }, Qt::SingleShotConnection);

    connect(ui->buttons, &QDialogButtonBox::rejected, this, [this](){
        edited = false;
        this->close();
    }, Qt::SingleShotConnection);


}

DialogEditSubscription::~DialogEditSubscription() {
    delete ui;
}

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


const QString invalid = QObject::tr("Invalid");

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
                auto bean = 
                    Configs::profileManager->GetProfile(profile_id);
                if (bean != nullptr){
                    switch(column){
                        case 0:
                        return bean->type;
                        case 1:
                        return bean->DisplayAddress();
                        case 2:
                        return bean->name;
                    }
                } else {
                    switch(column){
                        case 0:
                        return invalid;
                        case 1:
                        return "::";
                        case 2:
                        return invalid;
                    }
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
            tabid = (int)groups.size();
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
    if (this->selected_id != profile) {
        emit select_proxy(profile);
    }
    this->selected_id = profile;
    this->ui->profile_label->setText(DialogEditGroup::get_proxy_name(profile, this->is_for_routeprofile));
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
    if (ent->id >= 0 && ent->is_subscription) { // already a group
        auto extra = ent->getExtra();

        auto urltext = ui->url->text();

        if (urltext.isEmpty()) {
            runOnUiThread([this](){
                QMessageBox::warning(this, tr("Warning"), tr("Please input URL"));
            });
            return;
        }

        extra->skip_auto_update = !ui->auto_update->isChecked();
        extra->url = urltext;
        extra->Save();
    }
    ent->name = ui->name->text();
    ent->archive = ui->archive->isChecked();

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

QString DialogEditGroup::get_proxy_name(int id, bool is_for_routeprofile ) {
    std::shared_ptr<Configs::ProxyEntity> profile;
    if (id < 0 ||
        (profile = Configs::profileManager->GetProfile(id)) == nullptr){
        if (is_for_routeprofile ){
            if (id == -1){
                return QCoreApplication::translate(
        "DialogGroupChooseProxy", "Proxy");
            } else if (id == -2){
                return QCoreApplication::translate(
        "DialogGroupChooseProxy", "Direct");
            } else if (id == -3){
                return QCoreApplication::translate(
        "DialogGroupChooseProxy", "Direct");
            }
        }
        return QCoreApplication::translate(
        "DialogGroupChooseProxy", "None");
    } else {
        QString str = profile->name;
        if (str.isEmpty()){
            return QString("ID: ")+ QString::number(id);
        } else {
            return str;
        }
    }
}
