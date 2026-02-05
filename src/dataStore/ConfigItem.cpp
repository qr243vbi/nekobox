#include "nekobox/dataStore/ConfigItem.hpp"
#include "libcore_types.h"
#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/dataStore/Utils.hpp"
#include <qbuffer.h>
#include <qcborcommon.h>
#include <qcontainerfwd.h>

static void _put_store(ConfJsMap _map, const QString &str, void *value,
                       std::shared_ptr<configItem> item, JsonStore *store) {
  item->name = str;
  item->ptr = (size_t)(value) - (size_t)(store);
  _map[Configs::hash(str)] = item;
}

#define GET_PTR_OR_RETURN                                                      \
  void *ptr = this->getPtr(store);                                             \
  if (ptr == nullptr)                                                          \
    return;
#define SET_NODE(X)                                                            \
  void X##Item::setNode(JsonStore *store, const QJsonValue &value)

QString readString(QDataStream &stream) {
  unsigned int i;
  stream.readRawData((char *)&i, sizeof(unsigned int));
  QByteArray array;
  array.resize(i);
  stream.readRawData(array.data(), i);
  return QString::fromUtf8(array);
}

void appendString(QByteArray &array, const QString &str) {
  QByteArray ar = str.toUtf8();
  unsigned int i = ar.size();
  array.append((char *)&i, sizeof(unsigned int));
  array.append(ar);
}

SET_NODE(int) {
  GET_PTR_OR_RETURN
  if (value.isDouble()) {
    *(int *)ptr = value.toInt();
  }
}

SET_NODE(long) {
  GET_PTR_OR_RETURN
  if (value.isDouble()) {
    *(long long *)ptr = value.toInteger();
  }
}

SET_NODE(str) {
  GET_PTR_OR_RETURN
  if (value.isString()) {
    *(QString *)ptr = value.toString();
  }
}

SET_NODE(bool) {
  GET_PTR_OR_RETURN
  if (value.isBool()) {
    *(bool *)ptr = value.toBool();
  }
}

SET_NODE(boolPtr) {
  GET_PTR_OR_RETURN
  if (value.isBool()) {
    **(bool **)ptr = value.toBool();
  }
}


SET_NODE(strList) {
  GET_PTR_OR_RETURN
  if (value.isArray()) {
    QJsonArray array = value.toArray();
    if (!array.isEmpty()) {
      *(QStringList *)ptr = QJsonArray2QListStr(array);
    }
  }
}

SET_NODE(intList) {
  GET_PTR_OR_RETURN
  if (value.isArray()) {
    QJsonArray array = value.toArray();
    if (!array.isEmpty()) {
      *(QList<int> *)ptr = QJsonArray2QListInt(array);
    }
  }
}

SET_NODE(strMap) {
  GET_PTR_OR_RETURN
  if (value.isObject()) {
    *(QVariantMap *)ptr = value.toObject().toVariantMap();
  }
}

SET_NODE(jsonStore) {
  GET_PTR_OR_RETURN
  JsonStore *st = *(JsonStore **)ptr;
  if (st != nullptr) {
    st->FromJson(value.toObject());
  }
}

SET_NODE(jsonShared) {
  GET_PTR_OR_RETURN
  std::shared_ptr<JsonStore> st = *(std::shared_ptr<JsonStore> *)ptr;
  if (st != nullptr) {
    st->FromJson(value.toObject());
  }
}


SET_NODE(jsonStoreList) {
  if (value.isArray()) {
    QJsonArray array = value.toArray();
    if (!array.isEmpty()) {
      auto list = (QJsonStoreListBase *)this->getPtr(store);
      if (list == nullptr) {
        return;
      }
      list->clear();
      for (auto ptr : array) {
        JsonStore *store = list->createJsonStore();
        store->FromJson(ptr.toObject());
        list->append(store);
      }
    }
  }
}

#define GET_NODE(X) QJsonValue X##Item::getNode(JsonStore *store)

GET_NODE(jsonStoreList) {
  auto list = (QJsonStoreListBase *)this->getPtr(store);
  QJsonArray array;
  for (auto st : *list) {
    array.append(st->ToJson());
  }
  return array;
}
GET_NODE(jsonStore) {
  JsonStore *st = *(JsonStore **)(this->getPtr(store));
  if (st != nullptr) {
    return st->ToJson();
  } else {
    return QJsonValue::Null;
  }
}
GET_NODE(jsonShared) {
  std::shared_ptr<JsonStore> st = *(std::shared_ptr<JsonStore> *)(this->getPtr(store));
  if (st != nullptr) {
    return st->ToJson();
  } else {
    return QJsonValue::Null;
  }
}
GET_NODE(strMap) {
  return QJsonObject::fromVariantMap(*(QVariantMap *)this->getPtr(store));
}
GET_NODE(intList) {
  return QListInt2QJsonArray(*(QList<int> *)this->getPtr(store));
}

GET_NODE(strList) {
  return QListStr2QJsonArray(*(QList<QString> *)this->getPtr(store));
}

GET_NODE(bool) { return *(bool *)getPtr(store); }

GET_NODE(boolPtr) { return **(bool **)getPtr(store); }

GET_NODE(str) { return *(QString *)getPtr(store); }

GET_NODE(long) { return *(long long *)getPtr(store); }

GET_NODE(int) { return *(int *)getPtr(store); }

#define CASE_TYPE(X)                                                           \
  case ConfigItemType::type_##X:                                               \
    return std::make_shared<X##Item>();

std::shared_ptr<configItem> Configs_ConfigItem::getConfigItem(int i) {
  switch (i) {
    CASE_TYPE(int)
    CASE_TYPE(long)
    CASE_TYPE(bool)
    CASE_TYPE(str)
    CASE_TYPE(strList)
    CASE_TYPE(intList)
    CASE_TYPE(jsonStore)
    CASE_TYPE(strMap)
    CASE_TYPE(jsonStoreList)
    CASE_TYPE(boolPtr)
    CASE_TYPE(jsonShared)
  default:
    return std::make_shared<jsonStoreItem>();
  }
};

QByteArray JsonStore::ToBytes(const QStringList &without){
    QByteArray byteArray;
    QBuffer buffer(&byteArray); // Create a buffer to write to QByteArray
    buffer.open(QIODevice::WriteOnly);
    QDataStream out(&buffer); 
    auto _map = this->_map();

    for (auto value: _map.values() ){
      if (value->name == nullptr){
        qDebug() << "INVALID ITEM ::: UNKNOWN NAME" ;
        continue;
      }
        if (value == nullptr){
          qDebug() << "INVALID ITEM :::" << value->name;
          continue;
        }
        QString name = value->name;
        auto key = Configs::hash(name);
        unsigned char type = value->type();
        if (!without.contains(name)){
            out << key;
            out << type;
            Bin bin;
            bin.item = value.get();
            bin.store = this;
            out << bin;
        }
    }
    buffer.close();
    return byteArray;
};

void JsonStore::FromBytes(const QByteArray &data) {
  QDataStream stream(data);
  auto _map = this->_map();
  while (!stream.atEnd()) {
    QByteArray key;
    stream >> key;
    unsigned char type;
    stream >> type;
    auto iter = _map.find(key);
    std::shared_ptr<configItem> value = nullptr;
    JsonStore *store = this;
    if (iter != _map.end()) {
      value = iter.value();
    }
    if (value.get() == nullptr || value->type() != type) {
      value = getConfigItem(type);
      store = nullptr;
    }
    if (value.get() == nullptr){
      qDebug() << "SOMETHING STRANGE HERE: JsonStore::FromBytes";
      qDebug() << type;
    }
    Bin bin;
    bin.item = value.get();
    bin.store = store;
    stream >> bin;
  }
};

#define PUT_STORE(X)                                                           \
  _put_store(_map, str, value, std::make_shared<X##Item>(), this);

void JsonStore::_put(ConfJsMap _map, const QString &str, int *value) {
  PUT_STORE(int)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, long long *value) {
  PUT_STORE(long)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, QString *value) {
  PUT_STORE(str)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, bool *value) {
  PUT_STORE(bool)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, QStringList *value) {
  PUT_STORE(strList)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, QList<int> *value) {
  PUT_STORE(intList)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, JsonStore **value) {
  PUT_STORE(jsonStore)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, QVariantMap *value) {
  PUT_STORE(strMap)
};
void JsonStore::_put(ConfJsMap _map, const QString &str,
                     QJsonStoreListBase *value) {
  PUT_STORE(jsonStoreList)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, bool **value) {
  PUT_STORE(boolPtr)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, std::shared_ptr<JsonStore>*value) {
  PUT_STORE(jsonShared)
};


#define SET_BIN(X)                                                             \
  void X##Item::deserialize(QDataStream &data, JsonStore *store)
#define GET_BIN(X)                                                             \
  void X##Item::serialize(QDataStream &data, JsonStore *store) const

GET_BIN(int) { data << *(int *)this->getPtr(store); }
GET_BIN(long) { data << *(long long *)this->getPtr(store); }
GET_BIN(str) { data << *(QString *)this->getPtr(store); }
GET_BIN(bool) { data << *(bool *)this->getPtr(store); }
GET_BIN(boolPtr) { data << **(bool **)this->getPtr(store); }
GET_BIN(strList) { data << *(QStringList *)this->getPtr(store); }
GET_BIN(intList) { data << *(QList<int> *)this->getPtr(store); }
GET_BIN(strMap) { data << *(QVariantMap *)this->getPtr(store); }
GET_BIN(jsonStore) {
  QByteArray array;
  JsonStore *st = *(JsonStore **)this->getPtr(store);
  if (st != nullptr) {
    array = st->ToBytes();
  }
  data << array;
}
GET_BIN(jsonShared) {
  QByteArray array;
  std::shared_ptr<JsonStore> st = *(std::shared_ptr<JsonStore>*)this->getPtr(store);
  if (st != nullptr) {
    array = st->ToBytes();
  }
  data << array;
}
GET_BIN(jsonStoreList) {
  QList<QByteArray> value;
  for (JsonStore *st : *(QJsonStoreListBase *)this->getPtr(store)) {
    value.append(st->ToBytes());
  }
  data << value;
}
SET_BIN(jsonStoreList) {
  QList<QByteArray> value;
  data >> value;
  GET_PTR_OR_RETURN;
  QJsonStoreListBase *base = (QJsonStoreListBase *)this->getPtr(store);
  base->clear();
  for (QByteArray &array : value) {
    JsonStore *st = base->createJsonStore();
    base->append(st);
    st->FromBytes(array);
  }
}
SET_BIN(jsonStore) {
  QByteArray value;
  data >> value;
  GET_PTR_OR_RETURN
  JsonStore *st = *(JsonStore **)ptr;
  if (st != nullptr) {
    st->FromBytes(value);
  }
}
SET_BIN(jsonShared) {
  QByteArray value;
  data >> value;
  GET_PTR_OR_RETURN
  std::shared_ptr<JsonStore> st = *(std::shared_ptr<JsonStore> *)ptr;
  if (st != nullptr) {
    st->FromBytes(value);
  }
}
SET_BIN(strMap) {
  QVariantMap value;
  data >> value;
  GET_PTR_OR_RETURN
  *(QVariantMap *)ptr = value;
}
SET_BIN(intList) {
  QList<int> value;
  data >> value;
  GET_PTR_OR_RETURN
  *(QList<int> *)ptr = value;
}
SET_BIN(strList) {
  QStringList value;
  data >> value;
  GET_PTR_OR_RETURN
  *(QStringList *)ptr = value;
}
SET_BIN(bool) {
  bool value;
  data >> value;
  GET_PTR_OR_RETURN
  *(bool *)ptr = value;
}
SET_BIN(boolPtr) {
  bool value;
  data >> value;
  GET_PTR_OR_RETURN
  qDebug() << value;
  qDebug() << **(bool **)ptr;
  **(bool **)ptr = value;
}
SET_BIN(str) {
  QString value;
  data >> value;
  GET_PTR_OR_RETURN
  *(QString *)ptr = value;
}
SET_BIN(long) {
  long long value;
  data >> value;
  GET_PTR_OR_RETURN
  *(long long *)ptr = value;
}
SET_BIN(int) {
  int value;
  data >> value;
  GET_PTR_OR_RETURN
  *(int *)ptr = value;
}
