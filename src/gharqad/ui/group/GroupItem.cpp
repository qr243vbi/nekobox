



#include <nekobox/ui/group/GroupItem.h>
#include <nekobox/ui/group/dialog_edit_group.h>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/sys/Settings.h>
#include <nekobox/configs/sub/GroupUpdater.hpp>

#include <QMessageBox>

#include <QInputDialog>
#include <QUrlQuery>
#include <QComboBox>
#include <QVBoxLayout>
#include <QLabel>
#include <nekobox/ui/mainwindow_interface.h>

#include <QDialogButtonBox>

#define getGroupName chooseUpdateGroup

QString getGroupName(bool * ok, bool * createNewGroup, const QString& content){
    QDialog dlg;
    dlg.setWindowTitle(QObject::tr("Subscription URL Detected"));
    dlg.setMinimumWidth(450);

    auto layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);

    auto titleLabel = new QLabel(QObject::tr("How would you like to import this subscription?"));
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto urlLabel = new QLabel(content);
    urlLabel->setWordWrap(true);
    urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    urlLabel->setStyleSheet("QLabel { background-color: palette(base); padding: 8px; border: 1px solid palette(mid); border-radius: 4px; }");
    layout->addWidget(urlLabel);

    auto combo = new QComboBox();
    combo->addItem(QObject::tr("Create new subscription group"));
    combo->addItem(QObject::tr("Update existing group"));
    combo->addItem(QObject::tr("Import as proxy links"));
    layout->addWidget(combo);

    auto nameLayout = new QHBoxLayout();
    auto nameLabel = new QLabel(QObject::tr("Group name:"));
    auto lineEdit = new QLineEdit;
    lineEdit->setPlaceholderText(QObject::tr("Auto-detect from subscription"));
    nameLayout->addWidget(nameLabel);
    nameLayout->addWidget(lineEdit);
    layout->addLayout(nameLayout);

    auto buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Import"));
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     &dlg, &QDialog::reject);

    QObject::connect(combo, &QComboBox::currentIndexChanged,
                     &dlg, [lineEdit, nameLabel](int index) {
                         bool showName = (index == 0);
                         lineEdit->setVisible(showName);
                         nameLabel->setVisible(showName);
                     });

    lineEdit->setVisible(true);
    nameLabel->setVisible(true);

    int index;

    if (dlg.exec() == QDialog::Accepted) {
        index = combo->currentIndex();
        *ok = true;
        if (index == 0){
            *createNewGroup = true;
            return lineEdit->text();
        } else if (index == 1){
            *createNewGroup = false;
            return "";
        } else {
            *createNewGroup = false;
            return "link";
        }
    } else {
        *ok = false;
        *createNewGroup = false;
        return "";
    }
}

QString ParseSubInfo(const QString &info) {
    if (info.trimmed().isEmpty()) return "";

    QString result;

    long long used = 0;
    long long total = 0;
    long long expire = 0;

    auto re0m = QRegularExpression("total=([0-9]+)").match(info);
    if (re0m.lastCapturedIndex() >= 1) {
        total = re0m.captured(1).toLongLong();
    } else {
        return "";
    }
    auto re1m = QRegularExpression("upload=([0-9]+)").match(info);
    if (re1m.lastCapturedIndex() >= 1) {
        used += re1m.captured(1).toLongLong();
    }
    auto re2m = QRegularExpression("download=([0-9]+)").match(info);
    if (re2m.lastCapturedIndex() >= 1) {
        used += re2m.captured(1).toLongLong();
    }
    auto re3m = QRegularExpression("expire=([0-9]+)").match(info);
    if (re3m.lastCapturedIndex() >= 1) {
        expire = re3m.captured(1).toLongLong();
    }

    result = QObject::tr("Used: %1 Remain: %2 Expire: %3")
                         .arg(ReadableSize(used), (total == 0) ? QString::fromUtf8("\u221E") : ReadableSize(total - used), DisplayTime(expire, QLocale::ShortFormat));

    return result;
}

GroupItem::GroupItem(QWidget *parent, const std::shared_ptr<Configs::Group> &ent, QListWidgetItem *item) : QWidget(parent), ui(new Ui::GroupItem) {
    ui->setupUi(this);
    this->setLayoutDirection(Qt::LeftToRight);

    this->parentWindow = parent;
    this->ent = ent;
    this->item = item;
    if (ent == nullptr) return;

    connect(this, &GroupItem::edit_clicked, this, &GroupItem::on_edit_clicked);
    connect(Subscription::groupUpdater, &Subscription::GroupUpdater::asyncUpdateCallback, this, [=,this](int gid) { if (gid == this->ent->id) refresh_data(); });

    refresh_data();
}

GroupItem::~GroupItem() {
    delete ui;
}

void GroupItem::refresh_data() {
    ui->name->setText(ent->name);

    auto profileCount = ent->Profiles().length();
    auto type = !ent->is_subscription ? tr("Basic") : tr("Subscription");
    if (ent->archive) type = tr("Archive") + " " + type;
    type += " (" + QString::number(profileCount) + ")";
    ui->type->setText(type);

    if (!ent->is_subscription) {
        ui->url->hide();
        ui->subinfo->hide();
        ui->update_sub->hide();
    } else {
        auto extra = ent->getExtra();
        auto url = extra->url;
        if (url.length() > 80) {
            url = url.left(37) + "..." + url.right(40);
        }
        ui->url->setText(url);
        ui->url->setToolTip(extra->url);
        QStringList info;
        if (extra->sub_last_update != 0) {
            info << tr("Updated: %1").arg(DisplayTime(extra->sub_last_update, QLocale::ShortFormat));
        }
        auto subinfo = ParseSubInfo(extra->info);
        if (!extra->info.isEmpty()) {
            info << subinfo;
        }
        if (info.isEmpty()) {
            ui->subinfo->hide();
        } else {
            ui->subinfo->show();
            ui->subinfo->setText(info.join(" | "));
        }
    }
    runOnThread(
        [=,this] {
            adjustSize();
            item->setSizeHint(sizeHint());
            dynamic_cast<QWidget *>(parent())->adjustSize();
        },
        this);
}

void GroupItem::on_update_sub_clicked() {
    if (this->ent->is_subscription) {
        auto ent = this->ent->getExtra();
        QString url = ent->url;

        Subscription::groupUpdater->AsyncUpdate(GetMainWindow()->post_update_job,
        url, &chooseUpdateGroup, ent->id);
    }
}

void GroupItem::on_edit_clicked() {
    auto dialog = new DialogEditGroup(ent, parentWindow);
    connect(dialog, &QDialog::finished, this, [=,this] {
        if (dialog->result() == QDialog::Accepted) {
            ent->Save();
            refresh_data();
            MW_dialog_message(Dialog_DialogManageGroups, "refresh" + QString::number(ent->id));
        }
        dialog->deleteLater();
    });
    dialog->show();
}

void GroupItem::on_remove_clicked() {
    if (Configs::profileManager->groups.size() <= 1) return;
    if (!Configs::windowSettings->ask_delete || (QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(ent->name)) ==
        QMessageBox::StandardButton::Yes)) {
        Configs::profileManager->DeleteGroup(ent->id);
        MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
        delete item;
    }
}
