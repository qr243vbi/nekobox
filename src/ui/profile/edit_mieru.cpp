#include "include/ui/profile/edit_mieru.h"
#include <QFileDialog>

#include "include/configs/proxy/MieruBean.hpp"

EditMieru::EditMieru(QWidget *parent) : QWidget(parent), ui(new Ui::EditMieru) {
    ui->setupUi(this);
}

EditMieru::~EditMieru() {
    delete ui;
}

void EditMieru::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->MieruBean();

    P_LOAD_STRING(username)
    P_LOAD_STRING(password)
    P_LOAD_COMBO_STRING(multiplexing)
    ui->port_range->setText(bean->serverPorts.join(","));
}

bool EditMieru::onEnd() {
    auto bean = this->ent->MieruBean();
    P_SAVE_STRING(username)
    P_SAVE_STRING(password)
    P_SAVE_COMBO_STRING(multiplexing)
    bean->serverPorts = ui->port_range->toPlainText().split(",");
    return true;
}