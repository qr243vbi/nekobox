

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
#include <QPainter>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QEvent>
#include <QTableView>
#include <QItemSelectionModel>
#include <QAbstractItemModel>
#include <QScrollBar>

#define SELECTION_KEEPER_ROLE Qt::UserRole + 3

SelectionKeeper::SelectionKeeper(QTableView* view, QAbstractItemModel* model)
    : QObject(view),
      m_view(view),
      m_model(model)
{
    auto sm = m_view->selectionModel();

    connect(sm, &QItemSelectionModel::selectionChanged,
            this, &SelectionKeeper::onSelectionChanged);

    connect(sm, &QItemSelectionModel::currentChanged,
            this, &SelectionKeeper::onCurrentChanged);

    connect(model, &QAbstractItemModel::modelReset,
            this, &SelectionKeeper::restoreSelection);

    connect(model, &QAbstractItemModel::layoutChanged,
            this, &SelectionKeeper::restoreSelection);
}

int SelectionKeeper::idFromIndex(const QModelIndex& idx) const
{
    QModelIndex src = idx;

    if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(m_model))
        src = proxy->mapToSource(idx);

    return src.data(SELECTION_KEEPER_ROLE).toInt();
}

QModelIndex SelectionKeeper::indexFromId(int id) const
{
    if (!m_model) return {};

    for (int r = 0; r < m_model->rowCount(); ++r)
    {
        QModelIndex src = m_model->index(r, 0);

        if (src.data(SELECTION_KEEPER_ROLE).toInt() == id)
        {
            if (auto proxy = qobject_cast<const QSortFilterProxyModel*>(m_model))
                return proxy->mapFromSource(src);

            return src;
        }
    }

    return {};
}

void SelectionKeeper::onSelectionChanged(const QItemSelection& selected,
                                         const QItemSelection& deselected)
{
    auto process = [&](const QItemSelection& sel, bool add)
    {
        for (const auto& range : sel)
        {
            for (int r = range.top(); r <= range.bottom(); ++r)
            {
                QModelIndex idx = m_view->model()->index(r, 0);
                int id = idFromIndex(idx);

                if (add) m_selectedIds.insert(id);
                else     m_selectedIds.remove(id);
            }
        }
    };

    process(deselected, false);
    process(selected, true);
}

void SelectionKeeper::onCurrentChanged(const QModelIndex& current,
                                       const QModelIndex&)
{
    if (!current.isValid()) return;

    m_currentId = idFromIndex(current);
}

void SelectionKeeper::restoreSelection()
{
    auto sm = m_view->selectionModel();
    if (!sm || !m_model) return;

    QItemSelection selection;
    QModelIndex current;

    // Save scroll state
    QScrollBar* vbar = m_view->verticalScrollBar();
    QScrollBar* hbar = m_view->horizontalScrollBar();

    int v = vbar ? vbar->value() : 0;
    int h = hbar ? hbar->value() : 0;

    for (int id : m_selectedIds)
    {
        QModelIndex idx = indexFromId(id);
        if (idx.isValid())
            selection.select(idx, idx);
    }

    if (m_currentId != -1)
        current = indexFromId(m_currentId);

    sm->select(selection,
               QItemSelectionModel::SelectionFlag::Select |
               QItemSelectionModel::Rows);

    if (current.isValid())
        sm->setCurrentIndex(current,
                            QItemSelectionModel::NoUpdate);


    // Restore scroll state
    if (vbar) vbar->setValue(v);
    if (hbar) hbar->setValue(h);
}

FilterHeader::FilterHeader(Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
{
    setSectionsMovable(true);
    connect(this, &QHeaderView::sectionResized,
            this, [this](int, int, int){ updatePositions(); });

    connect(this, &QHeaderView::sectionMoved,
            this, [this](int, int, int){ updatePositions(); });
}

void ColumnFilterProxy::setEnabled(bool enable){
    this->enabled = enable;
    if (!enable){
        this->m_filters.clear();
    }
}

void ColumnFilterProxy::setColumnFilter(int column, const QString& text)
{
    if (text.isEmpty())
        m_filters.remove(column);
    else
        m_filters[column] = text;
}

bool ColumnFilterProxy::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    if (!enabled || !sourceModel()){
        return true;
    }

    for (auto it = m_filters.begin(); it != m_filters.end(); ++it)
    {
        int col = it.key();
        const QString &pattern = it.value();
        
        QModelIndex idx = sourceModel()->index(row, col, parent);
        QString data = sourceModel()->data(idx).toString();

        if (!data.contains(pattern, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

void FilterHeader::setFilterCount(int count)
{
    qDeleteAll(m_filters);
    m_filters.clear();

    for (int i = 0; i < count; ++i) {
        QLineEdit *edit = new QLineEdit(this);
        edit->setPlaceholderText(tr("Filter"));
        edit->setClearButtonEnabled(true);

        connect(edit, &QLineEdit::textChanged, this,
                [this, i](const QString &text) {
                    emit filterChanged(i, text);
                });

        m_filters.append(edit);
    }

    updateGeometry();   // triggers sizeHint recalculation
    updatePositions();
}

QString FilterHeader::filterText(int column) const
{
    if (column < 0 || column >= m_filters.size()){
        return QString();
    }
    return m_filters[column]->text();
}

int FilterHeader::editorHeight() const
{
    if (m_spacing == 0){
        return 0;
    }
    if (m_filters.isEmpty()){
        return 0;
    }
    return m_filters.first()->sizeHint().height();
}

bool FilterHeader::setFiltersVisible(bool visible){
    if (visible){
        this->m_spacing = 1;
    } else {
        this->m_spacing = 0;
    }
    for (auto filter: this->m_filters){
        filter->setVisible(visible);
        if (!visible){
            filter->clear();
        }
    }
    return visible;
}

QSize FilterHeader::sizeHint() const
{
    QSize base = QHeaderView::sizeHint();

    int h = base.height();
    int eheight = editorHeight();
    if (h > 0){
        h += m_spacing;
        h += eheight;
    }
    base.setHeight(h);
    return base;
}

void FilterHeader::resizeEvent(QResizeEvent *event)
{
    QHeaderView::resizeEvent(event);
    updatePositions();
}

bool FilterHeader::event(QEvent *event)
{
    if (event->type() == QEvent::FontChange ||
        event->type() == QEvent::StyleChange) {
        updateGeometry();
        updatePositions();
    }
    return QHeaderView::event(event);
}

bool FilterHeader::filtersVisible() const
{
    return this->m_spacing != 0;
}

void FilterHeader::mousePressEvent(QMouseEvent *event)
{
    int x = event->pos().x();
    constexpr int clickMargin = 10; // px

    int h = editorHeight();
    int filterTop = height() - h - m_spacing;

    if (event->pos().y() >= filterTop) {
        QHeaderView::mousePressEvent(event);
        return;
    }

    int logicalIndex = logicalIndexAt(x);

    if (logicalIndex >= 0) {

        int left  = sectionViewportPosition(logicalIndex);
        int right = left + sectionSize(logicalIndex);

        bool insideClickableArea =
            x >= left + clickMargin &&
            x <= right - clickMargin;
        if (insideClickableArea)
        emit sectionClicked(logicalIndex);
        #ifdef DEBUG_MODE
        qDebug() << "Header clicked:" << logicalIndex;
        #endif
    }

    QHeaderView::mousePressEvent(event);
}

void FilterHeader::updatePositions()
{
    int h = editorHeight();

    if (m_filters.isEmpty() || h == 0){
        return;
    }

    int y = height();
    y -= h;
    y -= m_spacing;

    for (int i = 0; i < m_filters.size(); ++i) {
        int x = sectionViewportPosition(i);
        int w = sectionSize(i);
        m_filters[i]->setGeometry(x, y, w, h);
    }
}

void FilterHeader::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(viewport());

    int h = editorHeight();
    int textHeight = height();
    if (h > 0){
        textHeight -= h;
        textHeight -= m_spacing;
    }

    for (int i = 0; i < count(); ++i) {
        if (isSectionHidden(i)){
            continue;
        }
        QRect rect(
            sectionViewportPosition(i),
            0,
            sectionSize(i),
            height()
        );

        // Split into two areas
        QRect textRect = rect;
        textRect.setHeight(textHeight);

        // Draw header section (ONLY text area)
        QStyleOptionHeader option;
        initStyleOption(&option);

        option.rect = textRect;
        option.section = i;
        option.text = model()->headerData(i, orientation(), Qt::DisplayRole).toString();
        option.textAlignment = Qt::AlignCenter;

        style()->drawControl(QStyle::CE_Header, &option, &painter, this);
/*
        if (h > 0){
        // Optional separator line
            painter.setPen(Qt::gray);
            painter.drawLine(rect.left(), textHeight, rect.right(), textHeight);
        }
*/          
    }
}

void MyTableModel::capture(QTableView * view){
    this->m_view = view;

    view->setWordWrap(false);
    view->setAlternatingRowColors(false);
    view->setShowGrid(false);

    this->proxy = std::make_shared<ColumnFilterProxy>();
    proxy->setSourceModel(this);
    view->setModel(this);
    proxy->setDynamicSortFilter(false);

    this->filter = std::make_shared<FilterHeader>(Qt::Horizontal, view);
    FilterHeader *header = this->filter.get();
    view->setHorizontalHeader(header);
    header->setResizeContentsPrecision(25);
    header->setFilterCount(3);
    header->setFiltersVisible(false);
    header->setSortIndicatorShown(false);
    view->verticalHeader()->setSortIndicatorShown(false);
    this->keeper = std::make_shared<SelectionKeeper>(view, this);

    view->setDragEnabled(true);
    view->setAcceptDrops(true);
    view->setDropIndicatorShown(true);
    view->setDefaultDropAction(Qt::MoveAction);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);

    QObject::connect(header, &FilterHeader::filterChanged,
        proxy.get(), [this](int id, const QString &text)->void{
            this->proxy->setColumnFilter(id, text);
            this->refresh();
        });
}

MyTableModel::MyTableModel(QObject *parent): QAbstractTableModel(parent){
};

int MyTableModel::columnCount(const QModelIndex &parent) const {
    return 5;
};


int MyTableModel::data_id(const QModelIndex &index) const
{
    #ifdef DEBUG_MODE
   // qDebug() << "Valid Index? " << index.isValid();
    #endif

    if (!index.isValid()){
        return -1;
    }

    int column = index.column();
    int row = index.row();

    #ifdef DEBUG_MODE
  //  qDebug() << "Row Id" << row;
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
    auto &profiles = m_data->profiles;
    if (profiles.count() <= row){
        return -1;
    }
    if (row < 0){
        return -1;
    }
    return profiles.at(row);
}

const QString invalid = QObject::tr("Invalid");

QStringList MyTableModel::mimeTypes() const {
    QStringList types;
    types << "application/x-nekobox-proxy-row";
    return types;
}

QMimeData* MyTableModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = new QMimeData();
    
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    QSet<int> uniqueRows;
    for (const QModelIndex &index : indexes) {
        if (index.isValid()) {
            uniqueRows.insert(index.row());
        }
    }

    stream << (int)uniqueRows.size();
    for (int row : uniqueRows) {
        stream << row;
    }

    mimeData->setData("application/x-nekobox-proxy-row", encodedData);
    
    return mimeData;
}

Qt::DropActions MyTableModel::supportedDropActions() const {
    return Qt::MoveAction;
}

bool MyTableModel::dropMimeData(const QMimeData *data, Qt::DropAction action, 
                           int row, int column, const QModelIndex &parent) {
    
#ifdef DEBUG_MODE
    qDebug() << "drop is called";
#endif
    const QString targetFormat = "application/x-nekobox-proxy-row";

    if (action != Qt::MoveAction || !data->hasFormat(targetFormat)) {
#ifdef DEBUG_MODE
    qDebug() << "Something went wrong: " << (action != Qt::MoveAction) << !data->hasFormat(targetFormat);
#endif
        return false;
    }

    int targetRow;
    if (row != -1) {
        targetRow = row;
    } else if (parent.isValid()) {
        targetRow = parent.row();
    } else {
        targetRow = rowCount(QModelIndex());
    }

    QByteArray encodedData = data->data(targetFormat);
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    
    int uniqueRowCount = 0;
    stream >> uniqueRowCount; 
    
    if (uniqueRowCount <= 0) {
#ifdef DEBUG_MODE
    qDebug() << "row count =(" << uniqueRowCount << encodedData.size();
#endif
        return false;
    }

    QList<int> indexes;

    while (uniqueRowCount > 0){
        int sourceRow;
        stream >> sourceRow; 
        indexes << sourceRow;
        uniqueRowCount --;
    }
    std::sort(indexes.begin(), indexes.end());
    {
        int sourceRow = indexes[0];
        if (sourceRow < targetRow) {
            targetRow--; 
        }
    }

    int count = 0;

    for (int & sourceRow : indexes){
    if (sourceRow == targetRow) {
#ifdef DEBUG_MODE
    qDebug() << "source and target are the same" << sourceRow << targetRow;
#endif
        continue;
    }

    int visualTarget = (sourceRow < targetRow) ? targetRow + 1 : targetRow; 

    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), visualTarget)) {
#ifdef DEBUG_MODE
    qDebug() << "Something went wrong <beginMoveRows> ";
#endif
        continue;
    }

    auto m_table = m_data(); 
    QList<int> & m_tableData = m_table->profiles;

    if (sourceRow < targetRow) {
        m_tableData.insert(targetRow, m_tableData[sourceRow]);
        m_tableData.removeAt(sourceRow);
    } else {
        auto movedRowItem = m_tableData.takeAt(sourceRow);
        m_tableData.insert(targetRow, movedRowItem);
    }

    endMoveRows();
    count ++;
    }

    if (count > 0){
        m_data()->Save();
        return true;
    };
    return false;
}


QVariant MyTableModel::data(const QModelIndex &index, int role) const
{

   // qDebug() << "filter section:" << index.column();

    int row = data_id(index);


    if (role == SELECTION_KEEPER_ROLE){
        return row;
    }

    if (row < 0){
        return QVariant();
    }


    std::shared_ptr<Configs::ProxyEntity> person = Configs::profileManager->GetProfile(row);

    bool invalid_yes = false;

    if (person == nullptr){
        #ifdef DEBUG_MODE
        qDebug() << "Found invalid data: " << row ;
        #endif
        invalid_yes = true;
    }

    if (role != Qt::DisplayRole){
        if (role == Qt::ForegroundRole && !invalid_yes){
            switch (index.column()){
                case 3: {
                    return QBrush(DisplayLatencyColor(person.get()));
                }
                case 2:
                case 1:
                case 0:
                if (row == Configs::dataStore->started_id){
                    QColor green(0x32, 0xCD, 0x32);
                    return QBrush(green);
                }
                default:
                break;
            }
        } 
        return QVariant();
    }


    int column = index.column();
    if (invalid_yes){
    switch (column){
        case 0:
            return invalid;
        case 1:
            return "::";
        case 2:
            return invalid;
        case 3:
            return "";
        case 4:
            return "";
    }
    } else {
    switch (column) {
        case 0:
            return person->type;
        case 1:
            return person->DisplayAddress();
        case 2:
            return person->name;
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

Qt::ItemFlags MyTableModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) return Qt::NoItemFlags | Qt::ItemIsDropEnabled;
    /*
    if (index.row() == 0) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    }*/
    if (QGuiApplication::keyboardModifiers() & Qt::ControlModifier) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    } else {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }
}

int MyTableModel::rowCount(const QModelIndex &parent) const {
    return this->count();
};

int MyTableModel::count() const {
    auto grp = this->m_data();
    if (grp == nullptr){
        return 0;
    } else {
        return grp->profiles.count();
    }
}

bool MyTableModel::filterEnabled(){
    return this->filter->filtersVisible();
};

bool MyTableModel::setFilterEnabled(bool filter){
    if (filter){
        this->m_view->setModel(this->proxy.get());
    } else {
        this->m_view->setModel(this);
    }
    auto ret = this->filter->setFiltersVisible(filter);
    this->proxy->setEnabled(filter);
    this->refresh();
    return ret;
};

void MyTableModel::refresh(){
    if (this->count() != this->old_count){
        this->beginResetModel();
        this->endResetModel();
    }
    filter->refresh();
    this->m_view->doItemsLayout();
}

void FilterHeader::refresh(){
    this->updatePositions();
    this->updateGeometry();
    this->setFixedHeight(this->sizeHint().height());
}

std::shared_ptr<Configs::Group> MyTableModel::m_data() const {
    return Configs::profileManager->CurrentGroup();
}

QVariant MyTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  //  qDebug() << "filter section:" << section;

    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical){
        int data_id = this->data_id(section);
        if (data_id == Configs::dataStore->started_id){
            return "*";
        } 
        #ifdef DEBUG_MODE
        return data_id;
        #else
        return QString::number(section + 1) + "  ";
        #endif
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