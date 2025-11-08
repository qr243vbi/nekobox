#include "include/ui/profile/edit_anytls.h"

#include "include/configs/proxy/AnyTLSBean.hpp"

#include <QUuid>
#include <QRegularExpressionValidator>
#include "include/global/GuiUtils.hpp"

EditAnyTLS::EditAnyTLS(QWidget *parent) : QWidget(parent), ui(new Ui::EditAnyTLS) {
    ui->setupUi(this);
    ui->interval->setValidator(QRegExpValidator_Number);
    ui->timeout->setValidator(QRegExpValidator_Number);
    ui->min->setValidator(QRegExpValidator_Number);
}

EditAnyTLS::~EditAnyTLS() {
    delete ui;
}

void EditAnyTLS::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->AnyTLS();

    ui->password->setText(bean->password);
    ui->interval->setText(bean->idle_session_check_interval);
    ui->timeout->setText(bean->idle_session_timeout);
    ui->min->setText(Int2String(bean->min_idle_session));
}

bool EditAnyTLS::onEnd() {
    auto bean = this->ent->AnyTLS();

    bean->password = ui->password->text();
    bean->idle_session_check_interval = ui->interval->text();
    bean->idle_session_timeout = ui->timeout->text();
    bean->min_idle_session = ui->min->text().toInt();

    return true;
}
