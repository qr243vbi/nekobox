#include "nekobox/dataStore/ConfigItem.hpp"
#include "libcore_types.h"
#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/dataStore/Utils.hpp"
#include <qcborcommon.h>
#include <qcontainerfwd.h>

static void _put_store(ConfJsMap _map, const QString & str, void * value, 
        std::shared_ptr<configItem> item, JsonStore * store){
    item->name = str;
    item->ptr = (size_t)(value) - (size_t)(store);
    _map[Configs::hash(str)] = item;
}

#define SET_BIN(X) void X##Item::setBin(JsonStore * store, const Bin & value) 
#define SET_NODE(X) void X##Item::setNode(JsonStore * store, const QJsonValue & value) 


void JsonStore::FromBin(const Bin & value){
    if (value.type == ConfigItemType::type_jsonStore){
        auto map = _map();
        QDataStream stream(value.payload);
        while (true){
            unsigned short type;
            unsigned int size;
            QByteArray key;
            QByteArray payload;
            key.resize(16);
            stream.readRawData((char*)&type, sizeof(unsigned short));
            if (type == ConfigItemType::type_end){
                break;
            }
            stream.readRawData(key.data(), 16);
            stream.readRawData((char*)&size, sizeof(unsigned int));
            payload.resize(size);
            stream.readRawData(payload.data(), size);
            Bin bin;
            bin.payload = payload;
            bin.type = type;
            if (map.contains(key)){
                map[key]->setBin(this, bin);
            }
        }
    }
}

Bin JsonStore::ToBin(const QStringList &without ){
    Bin bin;
    bin.type = ConfigItemType::type_jsonStore;
    for (auto [key, value] : asKeyValueRange(_map())){
        if (!without.contains(value->name)){
            auto & payload = bin.payload;
            unsigned short type = value->type();
            payload.append((const char *)&type, sizeof(type));
            payload.append(key);
            Bin another = value->getBin(this);
            unsigned int size = another.payload.size();
            payload.append((const char *)&size, sizeof(unsigned int));
            payload.append(another.payload);
        }
    }
    bin.payload.append(ConfigItemType::type_end);
    return bin;
}

QString readString(QDataStream & stream){
            unsigned int i;
            stream.readRawData((char*)&i, sizeof(unsigned int));
            QByteArray array;
            array.resize(i);
            stream.readRawData(array.data(), i);
            return QString::fromUtf8(array);
}

void appendString(QByteArray & array, const QString & str){
    QByteArray ar = str.toUtf8();
    unsigned int i = ar.size();
    array.append((char*)&i, sizeof(unsigned int));
    array.append(ar);
}

SET_BIN(jsonStoreList){
    if (value.type == ConfigItemType::type_jsonStoreList){
        QDataStream stream(value.payload);
        QJsonStoreListBase * base = (QJsonStoreListBase*) this->getPtr(store);
        while (!stream.atEnd()){
            unsigned int i;
            stream.readRawData((char*)&i, sizeof(unsigned int));
            
            Bin bin;
            bin.type = ConfigItemType::type_jsonStore;
            bin.payload.resize(i);
            stream.readRawData(bin.payload.data(), i);

            JsonStore * st = base->createJsonStore();
            base->append(st);
            st->FromBin(bin);
        }
    }
}

SET_BIN(jsonStore){
    if (value.type == ConfigItemType::type_jsonStore){
        JsonStore * st = *(JsonStore**) this->getPtr(store);
        if (st != nullptr){
            st->FromBin(value);
        }
    }
}

SET_BIN(strMap){
    if (value.type == ConfigItemType::type_strMap){
        QVariantMap * m = (QVariantMap*) this->getPtr(store);
        QDataStream stream(value.payload);
        m->clear();
        while (!stream.atEnd()){
            auto key = readString(stream);
            auto value = readString(stream);
            m->insert(key, value);
        }
    }
}

SET_BIN(intList){
    if (value.type == ConfigItemType::type_intList){
        QList<int> * list = (QList<int>*)this->getPtr(store);
        QDataStream stream(value.payload);
        list->clear();
        while (!stream.atEnd()){
            int i;
            stream.readRawData((char*)&i, sizeof(int));
            list->append(i);
        }
    }
}

SET_BIN(strList){
    if (value.type == ConfigItemType::type_strList){
        QStringList *list = (QStringList*)this->getPtr(store);
        QDataStream stream(value.payload);
        list->clear();
        while (!stream.atEnd()){
            list->append(readString(stream));
        }
    }
}

SET_BIN(bool){
    if (value.type == ConfigItemType::type_bool){
        *(bool *) this->getPtr(store) = value.payload.at(0);
    }
}

SET_BIN(str){
    if (value.type == ConfigItemType::type_str){
        *(QString *) this->getPtr(store) = QString::fromUtf8(value.payload);
    }
}

SET_BIN(long){
    if (value.type == ConfigItemType::type_long){
        *(long long *) this->getPtr(store) = *(long long*)value.payload.data();
    }
}

SET_BIN(int){
    if (value.type == ConfigItemType::type_int){
        *(int*) this->getPtr(store) = *(int*)value.payload.data();
    } 
}

SET_NODE(int){
    if (value.isDouble()){
        * (int*)this->getPtr(store) = value.toInt();
    }
}

SET_NODE(long){
    if (value.isDouble()){
        * (long long*) this->getPtr(store) = value.toInteger();
    }
}

SET_NODE(str){
    if (value.isString()){
        * (QString*) this->getPtr(store) = value.toString();
    }
}

SET_NODE(bool){
    if (value.isBool()){
        * (bool *) this->getPtr(store) = value.toBool();
    }
}

SET_NODE(strList){
    if (value.isArray()){
        QJsonArray array = value.toArray();
        if (!array.isEmpty()){
            *(QStringList*) this->getPtr(store) =  QJsonArray2QListStr(array);
        }
    }
}

SET_NODE(intList){
    if (value.isArray()){
        QJsonArray array = value.toArray();
        if (!array.isEmpty()){
            *(QList<int>*) this->getPtr(store) =  QJsonArray2QListInt(array);
        }
    }
}

SET_NODE(strMap){
    if (value.isObject()){
        *(QVariantMap*) this->getPtr(store) = value.toObject().toVariantMap();
    }
}

SET_NODE(jsonStore){
    JsonStore * st = *(JsonStore**) this->getPtr(store);
    if (st != nullptr){
        st->FromJson(value.toObject());
    }
}

SET_NODE(jsonStoreList){
    if (value.isArray()){
        QJsonArray array = value.toArray();
        if (!array.isEmpty()){
            auto list = (QJsonStoreListBase*) this->getPtr(store);
            list->clear();
            for (auto ptr : array){
                JsonStore * store = list->createJsonStore();
                store->FromJson(ptr.toObject());
                list->append(store);
            }
        }
    }
}

#define GET_NODE(X) QJsonValue X##Item::getNode(JsonStore * store)
#define GET_BIN(X) Bin X##Item::getBin(JsonStore * store)

GET_NODE(jsonStoreList){
    auto list = (QJsonStoreListBase*) this->getPtr(store);
    QJsonArray array;
    for (auto st : *list){
        array.append(st->ToJson());
    }
    return array;
}
GET_NODE(jsonStore){
    JsonStore * st = *(JsonStore**) (this->getPtr(store));
    if (st != nullptr){
        return st->ToJson();
    } else {
        return QJsonValue::Null;
    }
}
GET_NODE(strMap){
    return QJsonObject::fromVariantMap( *(QVariantMap *) this->getPtr(store) );
}
GET_NODE(intList){
    return QListInt2QJsonArray(*(QList<int> *) this->getPtr(store));
}

GET_NODE(strList){
    return QListStr2QJsonArray(*(QList<QString> *) this->getPtr(store));
}

GET_NODE(bool){
    return *(bool*) getPtr(store);
}

GET_NODE(str){
    return *(QString*) getPtr(store);
}

GET_NODE(long){
    return *(long long*) getPtr(store);
}

GET_NODE(int) {
    return *(int*)getPtr(store);
}

GET_BIN(jsonStoreList){
    Bin bin;
    bin.type = ConfigItemType::type_jsonStoreList;
    QJsonStoreListBase * base = (QJsonStoreListBase*) this->getPtr(store);
    for (JsonStore * st: *base){
        Bin another = st->ToBin();
        unsigned int i = another.payload.size();
        bin.payload.append((char*)&i, sizeof(unsigned int));
        bin.payload.append(another.payload);
    }
    return bin;
}

GET_BIN(jsonStore){
    JsonStore * st = *(JsonStore**) this->getPtr(store);
    
    if (st != nullptr) return st->ToBin();
    else {
        Bin bin;
        bin.type = ConfigItemType::type_jsonStore;
        bin.payload.append(ConfigItemType::type_end);
        return bin;
    }
}

GET_BIN(strMap){
    Bin bin;
    bin.type = ConfigItemType::type_strMap;
    QVariantMap variant = *(QVariantMap*) this->getPtr(store);
    for (auto [key, value]: asKeyValueRange(variant)){
        appendString(bin.payload, key);
        appendString(bin.payload, value.toString());
    }
    return bin;
}

GET_BIN(intList){
    Bin bin;
    bin.type = ConfigItemType::type_intList;
    QList<int> list = *(QList<int>*) this->getPtr(store);
    for (int i : list){
        bin.payload.append((char*)&i, sizeof(int));
    }
    return bin;
}

GET_BIN(strList){
    Bin bin;
    bin.type = ConfigItemType::type_strList;
    QList<QString> list = *(QList<QString>*) this->getPtr(store);
    for (QString i : list){
        appendString(bin.payload, i);
    }
    return bin;
}

GET_BIN(bool){
    Bin bin;
    bin.type = ConfigItemType::type_bool;
    bin.payload.append(*(bool*)this->getPtr(store));
    return bin;
}

GET_BIN(str){
    Bin bin;
    bin.type = ConfigItemType::type_str;
    bin.payload.append(((QString*)this->getPtr(store))->toUtf8());
    return bin;
}

GET_BIN(long){
    Bin bin;
    bin.type = ConfigItemType::type_long;
    bin.payload.append( (char*)this->getPtr(store), sizeof(long long) );
    return bin;
}

GET_BIN(int){
    Bin bin;
    bin.type = ConfigItemType::type_int;
    bin.payload.append( (char*)this->getPtr(store), sizeof(int) );
    return bin;
}

#define PUT_STORE(X) _put_store(_map, str, value, std::make_shared<X##Item>(), this);

        void JsonStore::_put(ConfJsMap _map, const QString& str, int* value){
            PUT_STORE(int)
        };
        void JsonStore::_put(ConfJsMap _map, const QString& str, long long* value){
            PUT_STORE(long)
        };
        void JsonStore::_put(ConfJsMap _map, const QString& str, QString* value){
            PUT_STORE(str)
        };
        void JsonStore::_put(ConfJsMap _map, const QString& str, bool * value){
            PUT_STORE(bool)
        };
        void JsonStore::_put(ConfJsMap _map, const QString& str, QStringList * value){
            PUT_STORE(strList)
        };
        void JsonStore::_put(ConfJsMap _map, const QString& str, QList<int> * value){
            PUT_STORE(intList)
        };
        void JsonStore::_put(ConfJsMap _map, const QString& str, JsonStore ** value){
            PUT_STORE(jsonStore)
        };
        void JsonStore::_put(ConfJsMap _map, const QString& str, QVariantMap * value){
            PUT_STORE(strMap)
        };
        void JsonStore::_put(ConfJsMap _map, const QString& str, QJsonStoreListBase * value){
            PUT_STORE(jsonStoreList)
        };

