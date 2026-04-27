#pragma once

#include <QAbstractTableModel>
#include <QVariant>
#include <nekobox/dataStore/Group.hpp>
#include <QTableView>
#include <qtableview.h>

#include <QObject>
#include <QTableView>
#include <QItemSelection>
#include <QPersistentModelIndex>
#include <QSet>

class MyTableModel;

class SelectionKeeper : public QObject {
    Q_OBJECT

public:
    explicit SelectionKeeper(QTableView* view, MyTableModel * model);

private slots:
    void onSelectionChanged(const QItemSelection& selected,
                            const QItemSelection& deselected);

    void onCurrentChanged(const QModelIndex& current,
                          const QModelIndex& previous);

    void restoreSelection();

private:
    void setup();
    QModelIndex toSource(const QModelIndex& index) const;
    QModelIndex fromSource(const QModelIndex& index) const;

private:
    QTableView* m_view = nullptr;
    MyTableModel * m_idRole;

    // selection state (by ID + persistent fallback)
    QSet<int> m_selectedIds;
    QSet<QPersistentModelIndex> m_selectedPersistent;

    // current index state
    QPersistentModelIndex m_currentPersistent;
    int m_currentId;
};


class MyTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MyTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    int data_id(const QModelIndex &index) const;
    int data_id(int row) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void refresh();
    void capture(QTableView*view);
private:
    std::shared_ptr<Configs::Group> m_data() const;
    int count() const ;
    int old_count = -1;
    std::shared_ptr<SelectionKeeper> keeper;
};

/*
beginResetModel();
  this->tableModel->endResetModel();
*/