

#pragma once

#include <QAbstractTableModel>
#include <QVariant>
#include <nekobox/dataStore/Group.hpp>
#include <QTableView>
#include <qtableview.h>

#include <QObject>
#include <QTableView>
#include <QStyledItemDelegate>
#include <QItemSelection>
#include <QPersistentModelIndex>
#include <QSet>
#include <QHeaderView>
#include <QLineEdit>
#include <QVector>

#include <QSortFilterProxyModel>
#include <QHash>

#define SELECTION_KEEPER_ROLE Qt::UserRole + 3

class ColumnFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    void setColumnFilter(int column, const QString& text);
    void setEnabled(bool enable);

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    QHash<int, QString> m_filters;
    bool enabled = false;
};

class SelectionKeeper : public QObject
{
    Q_OBJECT

public:
    SelectionKeeper(QTableView* view);
    void clearSelectionKeeper();

private slots:
    void onSelectionChanged(const QItemSelection& selected,
                            const QItemSelection& deselected);

    void onCurrentChanged(const QModelIndex& current,
                          const QModelIndex& previous);

    void restoreSelection();

private:
    int idFromIndex(const QModelIndex& idx) const;
    QModelIndex indexFromId(int id) const;

private:
    QTableView* m_view = nullptr;
    QAbstractItemModel* m_model = nullptr;

    QSet<int> m_selectedIds;
    int m_currentId = -1;
};


class FilterHeader : public QHeaderView
{
    Q_OBJECT

public:
    explicit FilterHeader(Qt::Orientation orientation, QWidget *parent = nullptr);

    void setFilterCount(int count);
    QString filterText(int column) const;
    bool setFiltersVisible(bool visible); 
    void mousePressEvent(QMouseEvent *event) override;
    bool filtersVisible() const;
    friend class MyTableModel;

signals:
    void filterChanged(int column, const QString &text);

protected:
    QSize sizeHint() const override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    void updatePositions();
    int editorHeight() const;
    void refresh();
private:
    QVector<QLineEdit*> m_filters;
    int m_spacing = 1;
    
};


class MyTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MyTableModel(QObject *parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex &index) const  override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    int data_id(const QModelIndex &index) const;
    int data_id(int row) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void refresh();
    void capture(QTableView*view);
    bool filterEnabled();
    bool setFilterEnabled(bool filter);
    QMimeData* mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, 
                      int row, int column, const QModelIndex &parent) override;
private:
    std::shared_ptr<Configs::Group> m_data() const;
    int count() const ;
    int old_count = -1;
    QTableView * m_view;
    std::shared_ptr<FilterHeader> filter;
    std::shared_ptr<SelectionKeeper> keeper;
    std::shared_ptr<ColumnFilterProxy> proxy;
};

/*
beginResetModel();
  this->tableModel->endResetModel();
*/