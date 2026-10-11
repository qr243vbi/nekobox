



#include <nekobox/ui/group/GroupItem.h>
#include <nekobox/ui/group/dialog_edit_group.h>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/sys/Settings.h>
#include <nekobox/configs/sub/GroupUpdater.hpp>

#include <QMessageBox>
#include <limits>

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
    struct Field {
        qint64 value = 0;
        bool present = false;
        bool valid = false;
    };
    const auto readField = [&info](const QString &name) {
        Field field;
        const auto match = QRegularExpression(
            "(?:^|;)\\s*" + name + "\\s*=([^;]*)").match(info);
        field.present = match.hasMatch();
        if (field.present) {
            const auto text = match.captured(1).trimmed();
            if (QRegularExpression("^[0-9]+$").match(text).hasMatch()) {
                field.value = text.toLongLong(&field.valid);
            }
        }
        return field;
    };

    const auto upload = readField("upload");
    const auto download = readField("download");
    const auto total = readField("total");
    const auto expire = readField("expire");
    if (!upload.valid && !download.valid && !total.valid && !expire.valid) return "";

    QString usedText = "-";
    QString remainingText = "-";
    QString expiryText = "-";
    const auto maximum = (std::numeric_limits<qint64>::max)();
    // An omitted counter keeps the existing zero fallback; an invalid counter
    // must not make the displayed usage or remaining quota look trustworthy.
    const bool hasUsage = (upload.valid || download.valid)
        && (!upload.present || upload.valid) && (!download.present || download.valid)
        && upload.value <= maximum - download.value;
    if (hasUsage) {
        const auto used = upload.value + download.value;
        usedText = ReadableSize(used);
        if (total.valid && total.value > 0) remainingText = ReadableSize(total.value - used);
    }
    if (total.valid && total.value == 0) remainingText = QString::fromUtf8("\u221E");
    // Zero means no expiry. Do not turn absent/invalid expiry into Unix epoch,
    // or overflow DisplayTime's seconds-to-milliseconds conversion.
    if (expire.valid && expire.value > 0 && expire.value <= maximum / 1000) {
        const auto formatted = DisplayTime(expire.value, QLocale::ShortFormat);
        if (!formatted.isEmpty()) expiryText = formatted;
    }

    return QObject::tr("Used: %1 Remain: %2 Expire: %3")
        .arg(usedText, remainingText, expiryText);
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
