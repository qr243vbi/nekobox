#include "include/ui/profile/edit_wireguard.h"

EditWireguard::EditWireguard(QWidget *parent) : QWidget(parent), ui(new Ui::EditWireguard) {
    ui->setupUi(this);
}

EditWireguard::~EditWireguard() {
    delete ui;
}

void EditWireguard::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->Wireguard();

#ifndef Q_OS_LINUX
    adjustSize();
#endif

    ui->private_key->setText(bean->private_key);
    ui->public_key->setText(bean->peer->public_key);
    ui->preshared_key->setText(bean->peer->pre_shared_key);
    // auto reservedStr = bean->peer->reserved;
    // ui->reserved->setText(reservedStr);
    ui->persistent_keepalive->setText(Int2String(bean->peer->persistent_keepalive));
    ui->mtu->setText(Int2String(bean->mtu));
    ui->sys_ifc->setChecked(bean->system);
    ui->local_addr->setText(bean->address.join(","));
    ui->workers->setText(Int2String(bean->worker_count));

    ui->enable_amnezia->setChecked(bean->enable_amnezia);
    ui->junk_packet_count->setText(Int2String(bean->junk_packet_count));
    ui->junk_packet_min_size->setText(Int2String(bean->junk_packet_min_size));
    ui->junk_packet_max_size->setText(Int2String(bean->junk_packet_max_size));
    ui->init_packet_junk_size->setText(Int2String(bean->init_packet_junk_size));
    ui->response_packet_junk_size->setText(Int2String(bean->response_packet_junk_size));
    ui->init_packet_magic_header->setText(Int2String(bean->init_packet_magic_header));
    ui->response_packet_magic_header->setText(Int2String(bean->response_packet_magic_header));
    ui->underload_packet_magic_header->setText(Int2String(bean->underload_packet_magic_header));
    ui->transport_packet_magic_header->setText(Int2String(bean->transport_packet_magic_header));
}

bool EditWireguard::onEnd() {
    auto bean = this->ent->Wireguard();

    bean->private_key = ui->private_key->text();
    bean->peer->public_key = ui->public_key->text();
    bean->peer->pre_shared_key = ui->preshared_key->text();
    auto rawReserved = ui->reserved->text();
    // bean->reserved = {};
    // for (const auto& item: rawReserved.split(",")) {
    //     if (item.trimmed().isEmpty()) continue;
    //     bean->reserved += item.trimmed().toInt();
    // }
    bean->peer->persistent_keepalive = ui->persistent_keepalive->text().toInt();
    bean->mtu = ui->mtu->text().toInt();
    bean->system = ui->sys_ifc->isChecked();
    bean->address = ui->local_addr->text().replace(" ", "").split(",");
    bean->worker_count = ui->workers->text().toInt();

    bean->enable_amnezia = ui->enable_amnezia->isChecked();
    bean->junk_packet_count = ui->junk_packet_count->text().toInt();
    bean->junk_packet_min_size = ui->junk_packet_min_size->text().toInt();
    bean->junk_packet_max_size = ui->junk_packet_max_size->text().toInt();
    bean->init_packet_junk_size = ui->init_packet_junk_size->text().toInt();
    bean->response_packet_junk_size = ui->response_packet_junk_size->text().toInt();
    bean->init_packet_magic_header = ui->init_packet_magic_header->text().toInt();
    bean->response_packet_magic_header = ui->response_packet_magic_header->text().toInt();
    bean->underload_packet_magic_header = ui->underload_packet_magic_header->text().toInt();
    bean->transport_packet_magic_header = ui->transport_packet_magic_header->text().toInt();

    return true;
}