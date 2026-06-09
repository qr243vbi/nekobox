



#include <nekobox/ui/profile/edit_wireguard.h>

#include <nekobox/configs/proxy/WireguardBean.h>

EditWireguard::EditWireguard(QWidget *parent) : QWidget(parent), ui(new Ui::EditWireguard) {
    ui->setupUi(this);
}

EditWireguard::~EditWireguard() {
    delete ui;
}

void EditWireguard::onStart(std::shared_ptr<Configs::ProxyEntity> ent) {
    this->ent = ent;
    auto bean = ent->WireguardBean();

#ifndef Q_OS_UNIX
    adjustSize();
#endif

    ui->private_key->setText(bean->privateKey);
    ui->public_key->setText(bean->publicKey);
    ui->preshared_key->setText(bean->preSharedKey);

    auto reservedStr = bean->FormatReserved().replace("-", ",");
    ui->reserved->setText(reservedStr);
    ui->persistent_keepalive->setText(QString::number(bean->persistentKeepalive));
    ui->mtu->setText(QString::number(bean->MTU));
    ui->sys_ifc->setChecked(bean->useSystemInterface);
    ui->local_addr->setText(bean->localAddress.join(","));

    ui->enable_amnezia->setChecked(ent->type == "awg");

    P_LOAD_INT(junk_packet_count);
    P_LOAD_INT(junk_packet_min_size);
    P_LOAD_INT(junk_packet_max_size);

    P_LOAD_INT(init_packet_junk_size);
    P_LOAD_INT(response_packet_junk_size);
    P_LOAD_INT(cookie_reply_junk_size);
    P_LOAD_INT(transport_packet_junk_size);

    P_LOAD_STRING(init_packet_magic_header);
    P_LOAD_STRING(response_packet_magic_header);
    P_LOAD_STRING(cookie_reply_magic_header);
    P_LOAD_STRING(transport_packet_magic_header);

    P_LOAD_STRING(i1);
    P_LOAD_STRING(i2);
    P_LOAD_STRING(i3);
    P_LOAD_STRING(i4);
    P_LOAD_STRING(i5);
}

bool EditWireguard::onEnd() {
    auto bean = ent->unlock(ent->WireguardBean());

    bean->privateKey = ui->private_key->text();
    bean->publicKey = ui->public_key->text();
    bean->preSharedKey = ui->preshared_key->text();
    auto rawReserved = ui->reserved->text();
    bean->reserved = {};
    for (const auto& item: rawReserved.split(",")) {
        if (item.trimmed().isEmpty()) continue;
        bean->reserved += item.trimmed().toInt();
    }
    bean->persistentKeepalive = ui->persistent_keepalive->text().toInt();
    bean->MTU = ui->mtu->text().toInt();
    bean->useSystemInterface = ui->sys_ifc->isChecked();
    bean->localAddress = ui->local_addr->text().replace(" ", "").split(",");

    bean->enableAmnezia(ui->enable_amnezia->isChecked());

    P_SAVE_INT(junk_packet_count);
    P_SAVE_INT(junk_packet_min_size);
    P_SAVE_INT(junk_packet_max_size);

    P_SAVE_INT(init_packet_junk_size);
    P_SAVE_INT(response_packet_junk_size);
    P_SAVE_INT(cookie_reply_junk_size);
    P_SAVE_INT(transport_packet_junk_size);

    P_SAVE_STRING(init_packet_magic_header);
    P_SAVE_STRING(response_packet_magic_header);
    P_SAVE_STRING(cookie_reply_magic_header);
    P_SAVE_STRING(transport_packet_magic_header);

    P_SAVE_STRING(i1);
    P_SAVE_STRING(i2);
    P_SAVE_STRING(i3);
    P_SAVE_STRING(i4);
    P_SAVE_STRING(i5);

    return true;
}
