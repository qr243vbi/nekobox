
#include <nekobox/ui/profile/edit_snell.h>
#include <QFileDialog>

#include <nekobox/configs/proxy/SnellBean.hpp>

EditSnell::EditSnell(QWidget *parent) : QWidget(parent), ui(new Ui::EditSnell) {
    ui->setupUi(this);
    ui->obfs_mode->addItems(Preset::SingBox::ObfsMode);
}

EditSnell::~EditSnell() {
    delete ui;
}

void EditSnell::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = _ent->SnellBean();
    P_LOAD_STRING(psk);
    P_LOAD_BOOL(reuse);
    P_LOAD_STRING(obfs_host);
    P_LOAD_SPIN(version);
    P_LOAD_COMBO_STRING_PTR(obfs_mode);
}

bool EditSnell::onEnd() {
    auto bean = ent->unlock(ent->SnellBean());
    P_SAVE_STRING(psk);
    P_SAVE_BOOL(reuse);
    P_SAVE_STRING(obfs_host);
    P_SAVE_INT(version);
    P_SAVE_COMBO_STRING_PTR(obfs_mode)
    return true;
}
