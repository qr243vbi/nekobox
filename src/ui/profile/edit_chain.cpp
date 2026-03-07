#include <nekobox/ui/profile/edit_chain.h>

#include <nekobox/ui/mainwindow_interface.h>
#include <nekobox/ui/profile/ProxyItem.h>
#include <nekobox/ui/group/dialog_edit_group.h>
#include <nekobox/dataStore/Database.hpp>
#include <nekobox/configs/proxy/ChainBean.hpp>

EditChain::EditChain(QWidget *parent) : QWidget(parent), ui(new Ui::EditChain) {
    ui->setupUi(this);
}

EditChain::~EditChain() {
    delete ui;
}

void EditChain::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->ChainBean();

    for (auto id: bean->list) {
        AddProfileToListIfExist(id);
    }
}

bool EditChain::onEnd() {
    if (get_edit_text_name().isEmpty()) {
        MessageBoxWarning(software_name, tr("Name cannot be empty."));
        return false;
    }

    auto bean = this->ent->ChainBean();

    QList<int> idList;
    for (int i = 0; i < ui->listWidget->count(); i++) {
        idList << ui->listWidget->item(i)->data(114514).toInt();
    }
    bean->list = idList;

    return true;
}

void EditChain::on_select_profile_clicked() {
  //  get_edit_dialog()->hide();
    auto window = new DialogGroupChooseProxy(this);
    window->setWindowTitle(QCoreApplication::translate(
        "DialogGroupChooseProxy","Add proxy"));
    QObject::connect(window, &DialogGroupChooseProxy::set_proxy,
        this, [this](int id) {
            AddProfileToListIfExist(id);
    });
    window->show();
}

void EditChain::AddProfileToListIfExist(int profileId) {
    auto _ent = Configs::profileManager->GetProfile(profileId);
    if (_ent != nullptr && _ent->type != "chain" && _ent->type != "extracore") {
        auto wI = new QListWidgetItem();
        wI->setData(114514, profileId);
        auto w = new ProxyItem(this, _ent, wI);
        ui->listWidget->addItem(wI);
        ui->listWidget->setItemWidget(wI, w);
        // change button
        connect(w->get_change_button(), &QPushButton::clicked, w, [=,this] {
            auto window = new DialogGroupChooseProxy(this);
            window->setWindowTitle(QCoreApplication::translate(
                "DialogGroupChooseProxy","Replace %1 proxy").arg( w->ent->bean->DisplayName() ));
            QObject::connect(window, &DialogGroupChooseProxy::set_proxy,
                this, [w, this](int newId) {
                    ReplaceProfile(w, newId);
            });
            window->show();
        });
    }
}

void EditChain::ReplaceProfile(ProxyItem *w, int profileId) {
    auto _ent = Configs::profileManager->GetProfile(profileId);
    if (_ent != nullptr && _ent->type != "chain" && _ent->type != "extracore") {
        w->item->setData(114514, profileId);
        w->ent = _ent;
        w->refresh_data();
    }
}
