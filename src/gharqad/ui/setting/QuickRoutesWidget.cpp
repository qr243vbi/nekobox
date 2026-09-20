#include <nekobox/ui/setting/QuickRoutesWidget.h>

#include <nekobox/dataStore/DataStore.hpp>
#include <nekobox/dataStore/Database.hpp>
#include <nekobox/dataStore/ResourceEntity.hpp>
#include <nekobox/dataStore/RouteEntity.h>
#include <nekobox/global/GuiUtils.hpp>

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

QuickRoutesWidget::QuickRoutesWidget(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(6);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(makeSection(tr("Applications"), &processTable, tr("Application"), true));
    splitter->addWidget(makeSection(tr("Domains"), &domainTable, tr("Domain"), false));
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    auto *saveRow = new QHBoxLayout();
    auto *hint = new QLabel(tr("Application routes match the .exe name. Use Tun mode to capture apps that do not use the system proxy. Saving restarts the core."));
    hint->setEnabled(false);
    hint->setWordWrap(true);
    saveRow->addWidget(hint);
    saveRow->addStretch();
    saveButton = new QPushButton(tr("Save"));
    saveRow->addWidget(saveButton);
    root->addLayout(saveRow);

    connect(saveButton, &QPushButton::clicked, this, [this] { saveToStore(); });

    loadFromStore();
}

QWidget *QuickRoutesWidget::makeSection(const QString &title, QTableWidget **table,
                                        const QString &firstHeader, bool processSection) {
    auto *section = new QWidget();
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *header = new QHBoxLayout();
    header->addWidget(new QLabel(title));
    header->addStretch();
    auto *addButton = new QPushButton(tr("Add"));
    auto *removeButton = new QPushButton(tr("Remove"));
    header->addWidget(addButton);
    header->addWidget(removeButton);
    layout->addLayout(header);

    auto *created = new QTableWidget(0, 2, section);
    setupTable(created, firstHeader);
    layout->addWidget(created, 1);
    *table = created;

    connect(addButton, &QPushButton::clicked, this, [this, processSection] {
        if (processSection) {
            addProcessRow(browseForExecutable(), Configs::proxyID);
        } else {
            addDomainRow({}, Configs::proxyID);
        }
    });
    connect(removeButton, &QPushButton::clicked, this,
            [created] { removeSelectedRows(created); });

    return section;
}

void QuickRoutesWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    refreshOutboundCombos();
}

void QuickRoutesWidget::setupTable(QTableWidget *table, const QString &firstHeader) {
    table->setHorizontalHeaderLabels({firstHeader, tr("Outbound")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->setColumnWidth(1, 200);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
}

QList<QuickRoutesWidget::OutboundChoice> QuickRoutesWidget::collectOutbounds() {
    QList<OutboundChoice> list;
    list.append({Configs::proxyID, tr("proxy")});
    list.append({Configs::directID, tr("direct")});
    list.append({Configs::blockID, tr("block")});

    if (Configs::profileManager == nullptr) {
        return list;
    }
    for (const auto &gid : Configs::profileManager->groupsTabOrder) {
        auto group = Configs::profileManager->GetGroup(gid);
        if (group == nullptr) {
            continue;
        }
        for (const auto &pid : group->profiles) {
            auto profile = Configs::profileManager->GetProfile(pid);
            if (profile == nullptr) {
                continue;
            }
            auto name = profile->DisplayName();
            if (!group->name.isEmpty()) {
                name = group->name + " / " + name;
            }
            list.append({pid, name});
        }
    }
    return list;
}

void QuickRoutesWidget::fillOutboundCombo(QComboBox *combo, const QList<OutboundChoice> &choices,
                                          int selectedId) const {
    combo->clear();
    int selectedIndex = 0;
    bool found = false;
    for (const auto &choice : choices) {
        combo->addItem(choice.name, choice.id);
        if (choice.id == selectedId) {
            selectedIndex = combo->count() - 1;
            found = true;
        }
    }
    if (!found) {
        combo->addItem(tr("Missing outbound (%1)").arg(selectedId), selectedId);
        selectedIndex = combo->count() - 1;
    }
    combo->setCurrentIndex(selectedIndex);
}

QComboBox *QuickRoutesWidget::makeOutboundCombo(int selectedId) const {
    auto *combo = new QComboBox();
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    fillOutboundCombo(combo, collectOutbounds(), selectedId);
    return combo;
}

QString QuickRoutesWidget::browseForExecutable() {
    QString startPath;
    if (Configs::resourceManager != nullptr) {
        startPath = Configs::resourceManager->getLatestPath();
    }
    const auto selected = QFileDialog::getOpenFileName(this, tr("Select an application"), startPath,
#ifdef Q_OS_WIN
                                                       tr("Applications (*.exe);;All files (*.*)")
#else
                                                       QString()
#endif
    );
    if (selected.isEmpty()) {
        return {};
    }
    if (Configs::resourceManager != nullptr) {
        Configs::resourceManager->latest_path = QFileInfo(selected).absolutePath();
    }
    return QDir::toNativeSeparators(selected);
}

QWidget *QuickRoutesWidget::makeProcessEditor(const QString &path) {
    auto *holder = new QWidget();
    auto *layout = new QHBoxLayout(holder);
    layout->setContentsMargins(2, 0, 2, 0);
    layout->setSpacing(4);

    auto *edit = new QLineEdit(path);
    edit->setObjectName("process_path");
    edit->setPlaceholderText(tr("Full path to an .exe, or just its file name"));
    auto *browse = new QToolButton();
    browse->setText("...");
    browse->setToolTip(tr("Select an application"));

    connect(browse, &QToolButton::clicked, holder, [this, edit] {
        const auto selected = browseForExecutable();
        if (!selected.isEmpty()) {
            edit->setText(selected);
        }
    });

    layout->addWidget(edit, 1);
    layout->addWidget(browse);
    return holder;
}

QString QuickRoutesWidget::processPathAt(int row) const {
    auto *holder = processTable->cellWidget(row, 0);
    if (holder == nullptr) {
        return {};
    }
    auto *edit = holder->findChild<QLineEdit *>("process_path");
    return edit != nullptr ? edit->text().trimmed() : QString();
}

int QuickRoutesWidget::outboundAt(QTableWidget *table, int row) {
    auto *combo = qobject_cast<QComboBox *>(table->cellWidget(row, 1));
    if (combo == nullptr) {
        return Configs::proxyID;
    }
    return combo->currentData().toInt();
}

void QuickRoutesWidget::addProcessRow(const QString &path, int outboundId) {
    const int row = processTable->rowCount();
    processTable->insertRow(row);
    processTable->setCellWidget(row, 0, makeProcessEditor(path));
    processTable->setCellWidget(row, 1, makeOutboundCombo(outboundId));
}

void QuickRoutesWidget::addDomainRow(const QString &domain, int outboundId) {
    const int row = domainTable->rowCount();
    domainTable->insertRow(row);
    auto *edit = new QLineEdit(domain);
    edit->setPlaceholderText(tr("Domain keyword, for example: example.com"));
    domainTable->setCellWidget(row, 0, edit);
    domainTable->setCellWidget(row, 1, makeOutboundCombo(outboundId));
}

void QuickRoutesWidget::removeSelectedRows(QTableWidget *table) {
    auto rows = table->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(),
              [](const QModelIndex &a, const QModelIndex &b) { return a.row() > b.row(); });
    for (const auto &index : rows) {
        table->removeRow(index.row());
    }
}

void QuickRoutesWidget::refreshOutboundCombos() {
    const auto choices = collectOutbounds();
    auto updateTable = [&](QTableWidget *table) {
        for (int row = 0; row < table->rowCount(); row++) {
            auto *combo = qobject_cast<QComboBox *>(table->cellWidget(row, 1));
            if (combo == nullptr) {
                continue;
            }
            const auto selected = combo->currentData().toInt();
            QSignalBlocker blocker(combo);
            fillOutboundCombo(combo, choices, selected);
        }
    };
    updateTable(processTable);
    updateTable(domainTable);
}

void QuickRoutesWidget::loadFromStore() {
    processTable->setRowCount(0);
    domainTable->setRowCount(0);
    if (Configs::dataStore == nullptr || Configs::dataStore->routing == nullptr ||
        Configs::dataStore->routing->quick_routes == nullptr) {
        return;
    }
    auto routes = Configs::dataStore->routing->quick_routes;
    routes->normalize();
    for (int i = 0; i < routes->process_match.size(); i++) {
        addProcessRow(routes->process_match[i], routes->process_outbound[i]);
    }
    for (int i = 0; i < routes->domain_match.size(); i++) {
        addDomainRow(routes->domain_match[i], routes->domain_outbound[i]);
    }
}

void QuickRoutesWidget::saveToStore() {
    if (Configs::dataStore == nullptr || Configs::dataStore->routing == nullptr ||
        Configs::dataStore->routing->quick_routes == nullptr) {
        return;
    }
    auto routes = Configs::dataStore->routing->quick_routes;
    routes->process_match.clear();
    routes->process_outbound.clear();
    routes->domain_match.clear();
    routes->domain_outbound.clear();

    for (int row = 0; row < processTable->rowCount(); row++) {
        const auto path = processPathAt(row);
        if (path.isEmpty()) {
            continue;
        }
        routes->process_match.append(path);
        routes->process_outbound.append(outboundAt(processTable, row));
    }
    for (int row = 0; row < domainTable->rowCount(); row++) {
        auto *edit = qobject_cast<QLineEdit *>(domainTable->cellWidget(row, 0));
        const auto domain = edit != nullptr ? edit->text().trimmed() : QString();
        if (domain.isEmpty()) {
            continue;
        }
        routes->domain_match.append(domain);
        routes->domain_outbound.append(outboundAt(domainTable, row));
    }

    Configs::dataStore->routing->Save();

    if (MW_show_log) {
        MW_show_log(tr("[Routes] Saved %1 application and %2 domain routes")
                        .arg(routes->process_match.size())
                        .arg(routes->domain_match.size()));
    }
    if (MW_dialog_message) {
        MW_dialog_message("", "UpdateDataStore,RouteChanged");
    }
}
