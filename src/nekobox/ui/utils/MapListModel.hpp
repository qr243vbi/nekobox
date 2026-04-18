#ifdef _WIN32
#include <winsock2.h>
#endif

#ifndef MAPLISTMODEL_H
#define MAPLISTMODEL_H

#include <QAbstractListModel>
#include <map>
#include <QVariant>
#include <functional>


#define ACCEPT_DATA_ROLE 908070

template <typename KeyType, typename ValueType>
class MapListModel : public QAbstractListModel {

public:
    MapListModel(std::function<QVariant(typename std::map<KeyType, ValueType>::const_iterator, int)> converter,
                 std::map<KeyType, ValueType> *dataMap,
                 QObject *parent = nullptr)
        : QAbstractListModel(parent), dataMap(dataMap), toVariant(converter) {}

    // Override the necessary methods
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
#ifdef DEBUG_MODE
    qDebug() << "MAP LIST MODEL ASK TO SIZE" << dataMap->size();
#endif
        if (parent.isValid()) {
            return 0; // REQUIRED for list models
        }
        return static_cast<int>(dataMap->size());
    }

    std::map<KeyType, ValueType>::const_iterator map_data(int index_row) const {
        auto it = dataMap->begin();
        std::advance(it, index_row);
        return it;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
            return QVariant();
        }
        auto it = map_data(index.row());
        return toVariant(it, role);  // Use custom function, passing iterator and role
    }

    void insert(const KeyType &key, const ValueType &value) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        dataMap->insert(key, value);
        endInsertRows();
    }

private:
    std::map<KeyType, ValueType> *dataMap;
    std::function<QVariant(typename std::map<KeyType, ValueType>::const_iterator, int)> toVariant; // Function pointer for conversion
};



#endif // MAPLISTMODEL_H






#ifndef GENERICQLISTMODEL_H
#define GENERICQLISTMODEL_H

#include <QAbstractListModel>
#include <QVariant>
#include <QList>
#include <functional>

template <typename T>
class ListPtrModel : public QAbstractListModel
{
public:
    using Converter = std::function<QVariant(const T&, int)>;

    explicit ListPtrModel(Converter converter,
                        QList<T>* data,
                        QObject* parent = nullptr)
        : QAbstractListModel(parent),
          m_data(data),
          m_converter(converter)
    {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid())
            return 0;
        return m_data ? m_data->size() : 0;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!m_data || !index.isValid() ||
            index.row() < 0 ||
            index.row() >= m_data->size())
            return {};

        return m_converter(m_data->at(index.row()), role);
    }

    void insert(const T& value) {
        int row = rowCount();
        insert(row, value);
    }

    void insert(int row, const T& value) {
        auto count = rowCount();
        if (row < 0 || row > count)
            return;

        #ifdef DEBUG_MODE
            qDebug() << "Insert to " << row;
        #endif

        beginInsertRows(QModelIndex(), row, row);
        if (row != count){
            m_data->insert(row, value);
        } else {
            m_data->append(value);
        }
        endInsertRows();
    }

    void remove(int row) {
        if (row < 0 || row >= rowCount())
            return;

        beginRemoveRows(QModelIndex(), row, row);
        m_data->removeAt(row);
        endRemoveRows();
    }

    void clear() {
        beginResetModel();
        m_data->clear();
        endResetModel();
    }

    void update(int row) {
        if (row < 0 || row >= rowCount())
            return;

        QModelIndex idx = index(row);
        emit dataChanged(idx, idx);
    }

private:
    QList<T>* m_data;
    Converter m_converter;
};

#endif // GENERICQLISTMODEL_H