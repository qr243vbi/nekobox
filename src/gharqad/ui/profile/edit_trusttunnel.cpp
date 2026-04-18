#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/ui/profile/edit_trusttunnel.h>
#include <nekobox/configs/proxy/TrustTunnelBean.hpp>

EditTrustTunnel::EditTrustTunnel(QWidget *parent) : QWidget(parent),
ui(new Ui::EditTrustTunnel) {
    ui->setupUi(this);
    ui->quic_congestion_control->addItem(tr("Off"));
    ui->quic_congestion_control->addItems(Preset::SingBox::QUICCongestionControlAlgorithm);
}

EditTrustTunnel::~EditTrustTunnel() {
    delete ui;
}

void EditTrustTunnel::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->TrustTunnelBean();
    P_LOAD_STRING(username)
    P_LOAD_STRING(password)
    if (bean->quic){
        P_LOAD_COMBO_STRING_PTR(quic_congestion_control)
    }
    P_LOAD_BOOL(health_check)
}

bool EditTrustTunnel::onEnd() {
    auto bean = ent->unlock(ent->TrustTunnelBean());
    P_SAVE_STRING(username)
    P_SAVE_STRING(password)
    if ((bean->quic = ui->quic_congestion_control->currentIndex() > 0)){
        P_SAVE_COMBO_STRING_PTR(quic_congestion_control)
    }
    P_SAVE_BOOL(health_check)
    return true;
}

