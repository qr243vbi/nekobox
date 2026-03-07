#include "nekobox/ui/profile/edit_tor.h"
#include <QFileDialog>

#include "nekobox/configs/proxy/TorBean.hpp"
#include "nekobox/global/GuiUtils.hpp"

EditTor::EditTor(QWidget *parent) : QWidget(parent), ui(new Ui::EditTor) {
    ui->setupUi(this);
}

EditTor::~EditTor() {
    delete ui;
}

void EditTor::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->TorBean();
    P_LOAD_STRINGLIST(extra_args)
    P_LOAD_STRING(executable_path)
    P_LOAD_STRING(data_directory)
    P_LOAD_STRINGMAP(torrc)
}

bool EditTor::onEnd() {
    auto bean = this->ent->TorBean();
    P_SAVE_STRING(executable_path)
    P_SAVE_STRING(data_directory)
    P_SAVE_STRINGLIST(extra_args)
    P_SAVE_STRINGMAP(torrc)
    return true;
}
