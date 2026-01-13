#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/dataStore/Utils.hpp"
#include <qcontainerfwd.h>

static void _put_store(ConfJsMap _map, const QString & str, void * value, 
        std::shared_ptr<configItem> item, JsonStore * store){
    item->name = str;
    item->ptr = (size_t)(value) - (size_t)(store);
    _map[Configs::hash(str)] = item;
}

#define SET_NODE(X) void X##Item::setNode(JsonStore * store, const QJsonValue & value) 

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
            *(QStringList*) this->getPtr(store) =  QJsonArray2QListString(array);
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
            for (auto ptr : array){
                JsonStore * store = list->createJsonStore();
                store->FromJson(ptr.toObject());
                list->append(store);
            }
        }
    }
}

#define GET_NODE(X) QJsonValue X##Item::getNode(JsonStore * store)
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

