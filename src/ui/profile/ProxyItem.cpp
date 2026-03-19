#include "nekobox/ui/profile/ProxyItem.h"

#include <QMessageBox>

ProxyItem::ProxyItem(QWidget *parent, const std::shared_ptr<Configs::ProxyEntity> &ent, QListWidgetItem *item)
    : QWidget(parent), ui(new Ui::ProxyItem) {
    ui->setupUi(this);
    this->setLayoutDirection(Qt::LeftToRight);

    this->item = item;
    this->ent = ent;
    if (ent == nullptr) return;

    refresh_data();
}

ProxyItem::~ProxyItem() {
    delete ui;
}

void ProxyItem::refresh_data() {
<<<<<<< HEAD
    ui->type->setText(ent->bean->DisplayType());
    ui->name->setText(ent->bean->DisplayName());
    ui->address->setText(ent->bean->DisplayAddress());
=======
    ui->type->setText(ent->DisplayType());
    ui->name->setText(ent->DisplayName());
    ui->address->setText(ent->DisplayAddress());
>>>>>>> other-repo/main
    ui->traffic->setText(ent->traffic_data->DisplayTraffic());
    ui->test_result->setText(ent->DisplayTestResult());

    runOnThread(
        [=,this] {
            adjustSize();
            item->setSizeHint(sizeHint());
            dynamic_cast<QWidget *>(parent())->adjustSize();
        },
        this);
}

void ProxyItem::on_remove_clicked() {
    if (!this->remove_confirm ||
<<<<<<< HEAD
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(ent->bean->DisplayName())) == QMessageBox::StandardButton::Yes) {
=======
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(
            ent->DisplayName())) == QMessageBox::StandardButton::Yes) {
>>>>>>> other-repo/main
        // TODO do remove (or not) -> callback
        delete item;
    }
}

QPushButton *ProxyItem::get_change_button() {
    return ui->change;
}
