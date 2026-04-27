#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/ui/mainwindow_table.h>
#include <nekobox/ui/mainwindow.h>
#include <nekobox/global/GuiUtils.hpp>
#include <nekobox/dataStore/Database.hpp>

#include <QItemSelectionModel>
#include <QAbstractItemModel>
#include <QAbstractProxyModel>
#include <qitemselectionmodel.h>
#include <qnamespace.h>

SelectionKeeper::SelectionKeeper(QTableView* view, MyTableModel* idRole)
    : QObject(view),
      m_view(view),
      m_idRole(idRole)
      //, m_idRole(idRole)
{
    m_view->horizontalHeader()->setSortIndicatorShown(false);
    m_view->verticalHeader()->setSortIndicatorShown(false);
    setup();
}

void SelectionKeeper::setup()
{
    if (!m_view || !m_view->model() || !m_view->selectionModel())
        return;

    auto sm = m_view->selectionModel();
    auto model = m_view->model();

    connect(sm, &QItemSelectionModel::selectionChanged,
            this, &SelectionKeeper::onSelectionChanged);

    connect(sm, &QItemSelectionModel::currentChanged,
            this, &SelectionKeeper::onCurrentChanged);

    connect(model, &QAbstractItemModel::modelReset,
            this, &SelectionKeeper::restoreSelection);

    connect(model, &QAbstractItemModel::rowsInserted,
            this, &SelectionKeeper::restoreSelection);

    connect(model, &QAbstractItemModel::rowsRemoved,
            this, &SelectionKeeper::restoreSelection);

    connect(model, &QAbstractItemModel::layoutChanged,
            this, &SelectionKeeper::restoreSelection);
}

QModelIndex SelectionKeeper::toSource(const QModelIndex& index) const
{
    QModelIndex idx = index;
    const QAbstractItemModel* model = m_view->model();

    while (auto proxy = qobject_cast<const QAbstractProxyModel*>(model)) {
        idx = proxy->mapToSource(idx);
        model = proxy->sourceModel();
    }

    return idx;
}

QModelIndex SelectionKeeper::fromSource(const QModelIndex& sourceIndex) const
{
    QModelIndex idx = sourceIndex;
    const QAbstractItemModel* model = m_view->model();

    QList<const QAbstractProxyModel*> proxies;

    while (auto proxy = qobject_cast<const QAbstractProxyModel*>(model)) {
        proxies.prepend(proxy);
        model = proxy->sourceModel();
    }

    for (auto proxy : proxies)
        idx = proxy->mapFromSource(idx);

    return idx;
}

void SelectionKeeper::onSelectionChanged(const QItemSelection& selected,
                                         const QItemSelection& deselected)
{
    for (const QModelIndex& idx : deselected.indexes()) {
        if (idx.column() != 0) continue;

        QModelIndex src = toSource(idx);
        int id = m_idRole->data_id(src.row());

        m_selectedIds.remove(id);
        m_selectedPersistent.remove(QPersistentModelIndex(src));
    }

    for (const QModelIndex& idx : selected.indexes()) {
        if (idx.column() != 0) continue;

        QModelIndex src = toSource(idx);
        int id = m_idRole->data_id(src.row());

        m_selectedIds.insert(id);
        m_selectedPersistent.insert(QPersistentModelIndex(src));
    }
}

void SelectionKeeper::onCurrentChanged(const QModelIndex& current,
                                       const QModelIndex& previous)
{
    Q_UNUSED(previous);

    QModelIndex src = toSource(current);

    m_currentPersistent = QPersistentModelIndex(src);
    m_currentId = m_idRole->data_id(src.row());
}

void SelectionKeeper::restoreSelection()
{
    auto sm = m_view->selectionModel();
    auto model = m_view->model();

    if (!model || model->rowCount() == 0)
        return;

    sm->blockSignals(true);
    sm->clearSelection();

    QItemSelection selection;

    // 1. Restore selection by persistent indexes
    for (const QPersistentModelIndex& pidx : m_selectedPersistent) {
        if (!pidx.isValid()) continue;

        QModelIndex viewIdx = fromSource(pidx);
        if (viewIdx.isValid()) {
            selection.select(viewIdx, viewIdx);
        }
    }

    // 2. Fallback to IDs if needed
    if (selection.isEmpty() && !m_selectedIds.isEmpty()) {
        for (int row = 0; row < model->rowCount(); ++row) {
            QModelIndex viewIdx = model->index(row, 0);
            QModelIndex srcIdx = toSource(viewIdx);

            if (m_selectedIds.contains(m_idRole->data_id(srcIdx.row()))) {
                selection.select(viewIdx, viewIdx);
            }
        }
    }

    if (!selection.isEmpty()) {
        sm->select(selection,
                   QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }

    // 3. Restore current index AFTER selection
    if (m_currentPersistent.isValid()) {
        sm->setCurrentIndex(fromSource(m_currentPersistent),
                            QItemSelectionModel::NoUpdate);
    } else if (!selection.isEmpty()) {
        sm->setCurrentIndex(selection.indexes().first(),
                            QItemSelectionModel::NoUpdate);
    }

    sm->blockSignals(false);
}

void MyTableModel::capture(QTableView * view){
    this->keeper = std::make_shared<SelectionKeeper>(view, this);
}

MyTableModel::MyTableModel(QObject *parent): QAbstractTableModel(parent){
};

int MyTableModel::columnCount(const QModelIndex &parent) const {
    return 5;
};

static std::shared_ptr<Configs::ProxyEntity> getProxyRow(int row){
    return Configs::profileManager->GetProfile(row);
}

int MyTableModel::data_id(const QModelIndex &index) const
{
    #ifdef DEBUG_MODE
    qDebug() << "Valid Index? " << index.isValid();
    #endif

    if (!index.isValid())
        return -1;

    int column = index.column();
    int row = index.row();

    #ifdef DEBUG_MODE
    qDebug() << "Row Id" << row;
    #endif

    if (!(column < 5 && row < this->count())){
        return -1;
    }
    return data_id(row);
}

int MyTableModel::data_id(int row) const {
    auto m_data = this->m_data();
    if (m_data == nullptr){
        return -1;
    }
    if (m_data->profiles.count() <= row){
        return -1;
    }
    return m_data->profiles[row];
}

QVariant MyTableModel::data(const QModelIndex &index, int role) const
{
    int row = data_id(index);
    #ifdef DEBUG_MODE
    qDebug() << "Row Id" << row;
    #endif

    if (role == Qt::UserRole + 1){
        return row;
    }

    if (row < 0){
        return QVariant();
    }


    std::shared_ptr<Configs::ProxyEntity> person = getProxyRow(row);

    bool invalid = false;

    if (person == nullptr){
        #ifdef DEBUG_MODE
        qDebug() << "Found invalid data: " << row ;
        #endif
        invalid = true;
    }

    if (role != Qt::DisplayRole){
        if (role == Qt::BackgroundRole && !invalid){
            if (row == Configs::dataStore->started_id){
                QColor green(0, 255, 0, 30);
                return QBrush(green);
            }
        } 
        if (role == Qt::ForegroundRole && !invalid){
            switch (index.column()){
                case 3: {
                    return QBrush(DisplayLatencyColor(person.get()));
                }
                default:
                break;
            }
        } 
        return QVariant();
    }


    int column = index.column();
    if (invalid){
    switch (column){
        case 0:
            return tr("Invalid");
        case 1:
            return "::";
        case 2:
            return tr("Invalid");
        case 3:
            return "";
        case 4:
            return "";
    }
    } else {
    switch (column) {
        case 0:
            return person->DisplayType();
        case 1:
            return person->DisplayAddress();
        case 2:
            return person->DisplayName();
        case 3:
            if (person->full_test_report.isEmpty()) {
                return person->DisplayTestResult();
            } else {
                return person->full_test_report;
            }
        case 4:
            return person->traffic_data->DisplayTraffic();
        default:
        break;
    }
    }
    return QVariant();
}

int MyTableModel::rowCount(const QModelIndex &parent) const {
    return this->count();
};

int MyTableModel::count() const {
    return this->m_data()->profiles.count();
}

void MyTableModel::refresh(){
    if (this->count() != this->old_count){
        this->beginResetModel();
        this->endResetModel();
    }
}

std::shared_ptr<Configs::Group> MyTableModel::m_data() const {
    return Configs::profileManager->CurrentGroup();
}

QVariant MyTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical){
        if (data_id(section) == Configs::dataStore->started_id){
            return "*";
        } 
        return section + 1;
    }

    if (orientation == Qt::Horizontal) {
        switch (section){
            case 0:
            return tr("Type");
            case 1:
            return tr("Address");
            case 2:
            return tr("Name");
            case 3:
            return tr("Test Result");
            case 4:
            return tr("Traffic");
        }
    }
    return QVariant();
}