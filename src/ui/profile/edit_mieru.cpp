#include "nekobox/ui/profile/edit_mieru.h"
#include <QFileDialog>

#include "nekobox/configs/proxy/MieruBean.hpp"

EditMieru::EditMieru(QWidget *parent) : QWidget(parent), ui(new Ui::EditMieru) {
    ui->setupUi(this);
<<<<<<< HEAD
=======
    this->ui->multiplexing->addItems(Preset::SingBox::MieruMultiplexing);
    this->ui->transport->addItems(Preset::SingBox::MieruTransport);
>>>>>>> other-repo/main
}

EditMieru::~EditMieru() {
    delete ui;
}

void EditMieru::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->MieruBean();

    P_LOAD_STRING(username)
    P_LOAD_STRING(password)
<<<<<<< HEAD
    P_LOAD_COMBO_STRING(transport)
    P_LOAD_COMBO_STRING(multiplexing)
=======
    P_LOAD_STRING(traffic_pattern)
    P_LOAD_COMBO_STRING_PTR(transport)
    P_LOAD_COMBO_STRING_PTR(multiplexing)
>>>>>>> other-repo/main
    ui->port_range->setText(bean->serverPorts.join(","));
}

bool EditMieru::onEnd() {
<<<<<<< HEAD
    auto bean = this->ent->MieruBean();
    P_SAVE_STRING(username)
    P_SAVE_STRING(password)
    P_SAVE_COMBO_STRING(transport)
    P_SAVE_COMBO_STRING(multiplexing)
=======
    auto bean = ent->unlock(ent->MieruBean());
    P_SAVE_STRING(username)
    P_SAVE_STRING(password)
    P_SAVE_STRING(traffic_pattern)
    P_SAVE_COMBO_STRING_PTR(transport)
    P_SAVE_COMBO_STRING_PTR(multiplexing)
>>>>>>> other-repo/main
    bean->serverPorts = ui->port_range->toPlainText().split(",");
    return true;
}
