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
        return dataMap->size();
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
