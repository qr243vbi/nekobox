#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/ui/profile/edit_trojan_vless.h>
#include <nekobox/configs/proxy/TrojanVLESSBean.hpp>
#include <nekobox/configs/proxy/Preset.hpp>

EditTrojanVLESS::EditTrojanVLESS(QWidget *parent) : QWidget(parent), ui(new Ui::EditTrojanVLESS) {
    ui->setupUi(this);
    flow_ = ui->flow;
}

EditTrojanVLESS::~EditTrojanVLESS() {
    delete ui;
}

void EditTrojanVLESS::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    std::shared_ptr<const Configs::TrojanVLESSBean> bean = this->ent->TrojanVLESSBean();
    if (bean->proxy_type == Configs::TrojanVLESSBean::proxy_VLESS) {
        ui->password_l->setText("UUID");
    }
    if (bean->proxy_type != Configs::TrojanVLESSBean::proxy_VLESS) {
        ui->flow->hide();
        ui->flow_l->hide();
        ui->encryption_l->hide();
        ui->encryption->hide();
    } else {
        ui->flow->addItems(Preset::SingBox::Flows );
        ui->flow->setCurrentText(bean->flow);
        ui->encryption->setText(bean->encryption);
    }
    ui->password->setText(bean->password);
    bean.reset();
}

bool EditTrojanVLESS::onEnd() {
    auto bean = ent->unlock(ent->TrojanVLESSBean());
    bean->password = ui->password->text();
    bean->flow = ui->flow->currentText();
    bean.reset();
    return true;
}
