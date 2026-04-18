#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/ui/profile/edit_juicity.h>
#include <nekobox/configs/proxy/JuicityBean.hpp>

EditJuicity::EditJuicity(QWidget *parent) : QWidget(parent),
ui(new Ui::EditJuicity) {
    ui->setupUi(this);
}

EditJuicity::~EditJuicity() {
    delete ui;
}

void EditJuicity::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->JuicityBean();
    P_LOAD_STRING(username)
    P_LOAD_STRING(password)
}

bool EditJuicity::onEnd() {
    auto bean = ent->unlock(ent->JuicityBean());
    P_SAVE_STRING(username)
    P_SAVE_STRING(password)
    return true;
}
