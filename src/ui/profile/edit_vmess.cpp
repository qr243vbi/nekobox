#include "nekobox/ui/profile/edit_vmess.h"

#include "nekobox/configs/proxy/VMessBean.hpp"

#include <QUuid>

EditVMess::EditVMess(QWidget *parent) : QWidget(parent), ui(new Ui::EditVMess) {
    ui->setupUi(this);
    connect(ui->uuidgen, &QPushButton::clicked, this, [=,this] { ui->uuid->setText(QUuid::createUuid().toString().remove("{").remove("}")); });
}

EditVMess::~EditVMess() {
    delete ui;
}

<<<<<<< HEAD
void EditVMess::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->VMessBean();
=======
void EditVMess::onStart(std::shared_ptr<Configs::ProxyEntity> ent) {
    this->ent = ent;
    auto bean = ent->VMessBean();
>>>>>>> other-repo/main

    ui->uuid->setText(bean->uuid);
    ui->aid->setText(QString::number(bean->aid));
    ui->security->setCurrentText(bean->security);
}

bool EditVMess::onEnd() {
<<<<<<< HEAD
    auto bean = this->ent->VMessBean();
=======
    auto bean = ent->unlock(ent->VMessBean());
>>>>>>> other-repo/main

    bean->uuid = ui->uuid->text();
    bean->aid = ui->aid->text().toInt();
    bean->security = ui->security->currentText();
<<<<<<< HEAD

=======
>>>>>>> other-repo/main
    return true;
}
