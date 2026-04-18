#ifdef _WIN32
#include <winsock2.h>
#endif


#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/ui/profile/edit_naive.h>
#include <nekobox/configs/proxy/NaiveBean.hpp>

EditNaive::EditNaive(QWidget *parent) : QWidget(parent),
ui(new Ui::EditNaive) {
    ui->setupUi(this);
    ui->quic_congestion_control->addItem(tr("Off"));
    ui->quic_congestion_control->addItems(Preset::SingBox::QUICCongestionControlAlgorithm);
}

EditNaive::~EditNaive() {
    delete ui;
}

void EditNaive::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->NaiveBean();
    P_LOAD_STRING(username)
    P_LOAD_STRING(password)
    SP_LOAD_INT(insecure_concurrency)
    ui->uot->setCurrentIndex(bean->uot);
    if (bean->quic){
        P_LOAD_COMBO_STRING_PTR(quic_congestion_control)
    }
    P_LOAD_STRINGMAP(extra_headers)
}

bool EditNaive::onEnd() {
    auto bean = ent->unlock(ent->NaiveBean());
    P_SAVE_STRING(username)
    P_SAVE_STRING(password)
    SP_SAVE_INT(insecure_concurrency)
    bean->uot = ui->uot->currentIndex();
    if ((bean->quic = ui->quic_congestion_control->currentIndex() > 0)){
        P_SAVE_COMBO_STRING_PTR(quic_congestion_control)
    }
    P_SAVE_STRINGMAP(extra_headers)
    return true;
}
