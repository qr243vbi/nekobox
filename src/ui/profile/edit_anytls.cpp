#include "nekobox/ui/profile/edit_anytls.h"

#include "nekobox/configs/proxy/AnyTLSBean.hpp"
#include "nekobox/ui/profile/edit_shadowtls.h"

#include "nekobox/configs/proxy/ShadowTLSBean.hpp"

#include <QUuid>
#include <QRegularExpressionValidator>
#include "nekobox/global/GuiUtils.hpp"

EditAnyTLS::EditAnyTLS(QWidget *parent) : QWidget(parent), ui(new Ui::EditAnyTLS) {
    ui->setupUi(this);
    ui->min->setValidator(QRegExpValidator_Number);
}

EditAnyTLS::~EditAnyTLS() {
    delete ui;
}

void EditAnyTLS::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->AnyTLSBean();

    ui->password->setText(bean->password);
    ui->interval->setText(bean->idle_session_check_interval);
    ui->timeout->setText(bean->idle_session_timeout);
    ui->min->setText(QString::number(bean->min_idle_session));
}

bool EditAnyTLS::onEnd() {
    auto bean = this->ent->AnyTLSBean();

    bean->password = ui->password->text();
    bean->idle_session_check_interval = ui->interval->text();
    bean->idle_session_timeout = ui->timeout->text();
    bean->min_idle_session = ui->min->text().toInt();

    return true;
}


EditShadowTLS::EditShadowTLS(QWidget *parent) : QWidget(parent), ui(new Ui::EditShadowTLS) {
    ui->setupUi(this);
}

EditShadowTLS::~EditShadowTLS() {
    delete ui;
}

void EditShadowTLS::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->ShadowTLSBean();

    ui->password->setText(bean->password);
    int ver = bean->shadowtls_version - 1;
    ver = (ver < 0) ? 0 : (ver > 3 ? 2 : ver);
    ui->comboBox->setCurrentIndex(ver);
}

bool EditShadowTLS::onEnd() {
    auto bean = this->ent->ShadowTLSBean();

    bean->password = ui->password->text();
    bean->shadowtls_version = ui->comboBox->currentIndex() + 1;
    return true;
}
