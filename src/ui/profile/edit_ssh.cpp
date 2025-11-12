#include "include/ui/profile/edit_ssh.h"
#include <QFileDialog>

#include "include/configs/proxy/SSHBean.h"

EditSSH::EditSSH(QWidget *parent) : QWidget(parent), ui(new Ui::EditSSH) {
    ui->setupUi(this);
}

EditSSH::~EditSSH() {
    delete ui;
}

void EditSSH::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->SSH();

    ui->user->setText(bean->user);
    ui->password->setText(bean->password);
    ui->private_key->setText(bean->private_key);
    ui->private_key_path->setText(bean->private_key_path);
    ui->private_key_pass->setText(bean->private_key_passphrase);
    ui->host_key->setText(bean->host_key.join(","));
    ui->host_key_algs->setText(bean->host_key_algorithms.join(","));
    ui->client_version->setText(bean->client_version);

    connect(ui->choose_pk, &QPushButton::clicked, this, [=,this] {
        auto fn = QFileDialog::getOpenFileName(this, QObject::tr("Select"), QDir::currentPath(),
                                               "", nullptr, QFileDialog::Option::ReadOnly);
        if (!fn.isEmpty()) {
            ui->private_key_path->setText(fn);
        }
    });
}

bool EditSSH::onEnd() {
    auto bean = this->ent->SSH();

    bean->user = ui->user->text();
    bean->password = ui->password->text();
    bean->private_key = ui->private_key->toPlainText();
    bean->private_key_path = ui->private_key_path->text();
    bean->private_key_passphrase = ui->private_key_pass->text();
    if (!ui->host_key->text().trimmed().isEmpty()) bean->host_key = ui->host_key->text().split(",");
    else bean->host_key = {};
    if (!ui->host_key_algs->text().trimmed().isEmpty()) bean->host_key_algorithms = ui->host_key_algs->text().split(",");
    else bean->host_key_algorithms = {};
    bean->client_version = ui->client_version->text();

    return true;
}