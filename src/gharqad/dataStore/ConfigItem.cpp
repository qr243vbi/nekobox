#include <memory>
#include <nekobox/dataStore/ConfigItem.hpp>
#include "libcore_types.h"
#include <nekobox/dataStore/Configs.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <qbuffer.h>
#include <qcborcommon.h>
#include <qcontainerfwd.h>

#include <boost/algorithm/string.hpp>
#include <qnamespace.h>
#include <qvariant.h>


static void _put_store(ConfJsMap _map, const QString &str, void *value,
                       std::shared_ptr<configItem> item, JsonStore *store) {
  item->name = str;
  item->ptr = (size_t)(value) - (size_t)(store);
  _map[Configs::hash(str)] = item;
}

#define GET_PTR_OR_RETURN                                                      \
  void *ptr = (void*)store;                                             \
  if (ptr == nullptr)                                                          \
    return;
#define SET_NODE(X)                                                            \
  void X##Item::setNode(size_t store, const QJsonValue &value)

#define COMPARE(X)                                                            \
  signed char X##Item::compare(size_t store, size_t other_store)

#define GET_PTR_TYPE2(T)                \
  auto a = (T *)(void*)(store);         \
  auto b = (T *)(void*)(other_store);   \

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

SET_NODE(double) {
  GET_PTR_OR_RETURN
  if (value.isDouble()) {
    *(double *)ptr = value.toDouble();
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
  } else if (value.isString()){
    auto val = QJsonDocument::fromJson(value.toString().toUtf8());
    *(QVariantMap *)ptr = val.object().toVariantMap();
  }
}

SET_NODE(jsonStore) {
  GET_PTR_OR_RETURN
  JsonStore *st = *(JsonStore **)ptr;
  if (st != nullptr) {
    st->FromJson(value.toObject());
  }
}

SET_NODE(enum) {
  GET_PTR_OR_RETURN
  std::shared_ptr<JsonEnum> st = *(std::shared_ptr<JsonEnum> *)ptr;
  if (st != nullptr) {
    *st = value;
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
  GET_PTR_OR_RETURN
  if (value.isArray()) {
    QJsonArray array = value.toArray();
    {
      auto list = (QJsonStoreListBase *)ptr;
      list->clear();
      for (auto ptr : array) {
        JsonStore *store = list->createJsonStore();
        store->FromJson(ptr.toObject());
        list->append(store);
      }
    }
  }
}

QJsonValue configItem::getNode(const JsonStore * store) { 
  return getNode((size_t)getPtr(store)); 
}
void configItem::setNode(const JsonStore * store, const QJsonValue &value) { 
  setNode((size_t)getPtr(store), value); 
}
void configItem::serialize(QDataStream &data, const JsonStore * store) const { 
  serialize(data, (size_t)getPtr(store)); 
}
void configItem::deserialize(QDataStream &data, const JsonStore * store) { 
  deserialize(data, (size_t)getPtr(store)); 
}
void configItem::SaveINI(const JsonStore * store, const QFileInfo& settings, const QString & path) { 
  SaveINI( (size_t)getPtr(store), settings, path ); 
}
void configItem::LoadINI(const JsonStore * store, const QFileInfo& settings, const QString & path) { 
  LoadINI( (size_t)getPtr(store), settings, path ); 
}

#define LOAD_CONF(X) void X##Item::LoadINI(size_t store, const QFileInfo& settings, const QString &path)
#define GET_PTR_TYPE(T) T * ptr = (T *) (void*)store;
#define GET_PTR_TYPE_INI(T) auto val =  QSettingsFromFileInfo(settings); GET_PTR_TYPE(T)


LOAD_CONF(int) {
  GET_PTR_TYPE_INI(int)
  *ptr = val.value(path + this->name, 0).toInt();
}

LOAD_CONF(double) {
  GET_PTR_TYPE_INI(double)
  *ptr = val.value( path + this->name, 0).toDouble();
}

LOAD_CONF(long) {
  GET_PTR_TYPE_INI(long long)
  *ptr = val.value( path + this->name, 0).toLongLong();
}

LOAD_CONF(bool) {
  GET_PTR_TYPE_INI(bool)
  *ptr = val.value( path + this->name, false).toBool();
}

LOAD_CONF(boolPtr) {
  GET_PTR_TYPE_INI(bool*)
  **ptr = val.value( path + this->name, false).toBool();
}

LOAD_CONF(str) {
  GET_PTR_TYPE_INI(QString)
  *ptr = val.value( path + this->name, "").toString();
}

LOAD_CONF(strList) {
  GET_PTR_TYPE_INI(QStringList)
  *ptr = val.value( path + this->name, {}).toStringList();
}

LOAD_CONF(intList) {
  GET_PTR_TYPE_INI(QList<int>)
  *ptr = QListStr2QListInt(val.value(path + this->name, {}).toStringList());
}

LOAD_CONF(strMap) {
  GET_PTR_TYPE_INI(QVariantMap)
  ptr->clear();
  val.beginGroup(path + this->name);
  for (auto & key : val.childKeys()){
    ptr->insert(key, val.value(key, ""));
  }
  val.endGroup();
}

LOAD_CONF(enum) {
  GET_PTR_TYPE_INI(std::shared_ptr<JsonEnum>)
  auto st = *ptr;
  if (st != nullptr) {
    st->set(val.value(path + this->name, "").toString());
  }
}

LOAD_CONF(jsonShared) {
  GET_PTR_TYPE_INI(std::shared_ptr<JsonStore>)
  auto st = *ptr;
  if (st != nullptr) {
    st->LoadINI(settings, path + this->name + "/");
  }
}
LOAD_CONF(jsonStore) {
  GET_PTR_TYPE_INI(JsonStore*)
  auto st = *ptr;
  if (st != nullptr) {
    st->LoadINI(settings, path + this->name + "/");
  }
}
LOAD_CONF(jsonStoreList) {
  GET_PTR_TYPE_INI(QJsonStoreListBase)
  int index = 0;
  QStringList keys;
  {
    auto val = QSettingsFromFileInfo(settings);
    val.beginGroup(path + this->name);
    keys = val.childGroups();
    val.endGroup();
  }
  index = keys.count();
  for (int i = 0; i < index; i ++){
    ptr->append(ptr->createJsonStore());
  }

  for (auto st : keys) {
    {
      bool ok;
      int key = st.toInt(&ok);
      if (ok){
        auto stt = ptr->value(key, nullptr) ;
        if (stt != nullptr) {
          stt->LoadINI(settings, path + this->name + "/" + st + "/");
        }
      } 
    }
  }
}

#define SAVE_CONF(X) void X##Item::SaveINI(size_t store, const QFileInfo& settings, const QString &path)

SAVE_CONF(int) {
  GET_PTR_TYPE_INI(int)
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(double) {
  GET_PTR_TYPE_INI(double)
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(long) {
  GET_PTR_TYPE_INI(long long)
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(bool) {
  GET_PTR_TYPE_INI(bool)
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(boolPtr) {
  GET_PTR_TYPE_INI(bool*)
  val.setValue(path + this->name, **ptr);
}

SAVE_CONF(str) {
  GET_PTR_TYPE_INI(QString)
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(strList) {
  GET_PTR_TYPE_INI(QStringList)
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(intList) {
  GET_PTR_TYPE_INI(QList<int>)
  val.setValue(path + this->name, QListInt2QListStr(*ptr));
}

SAVE_CONF(strMap) {
  GET_PTR_TYPE_INI(QVariantMap)
  val.beginGroup(path + this->name);
  val.remove("");
  for (auto [key, value] : asKeyValueRange(*ptr)){
    val.setValue(key, value);
  }
  val.endGroup();
}

SAVE_CONF(enum) {
  GET_PTR_TYPE_INI(std::shared_ptr<JsonEnum>)
  auto st = *ptr;
  if (st != nullptr) {
    val.setValue(path + this->name, (QString)*st);
  }
}

SAVE_CONF(jsonShared) {
  GET_PTR_TYPE_INI(std::shared_ptr<JsonStore>)
  auto st = *ptr;
  if (st != nullptr) {
    st->SaveINI(settings, path + this->name + "/");
  }
}
SAVE_CONF(jsonStore) {
  GET_PTR_TYPE_INI(JsonStore*)
  auto st = *ptr;
  if (st != nullptr) {
    st->SaveINI(settings, path + this->name + "/");
  }
}
SAVE_CONF(jsonStoreList) {
  GET_PTR_TYPE_INI(QJsonStoreListBase)
  int index = 0; 
  for (auto st : *ptr) {
    if (st != nullptr) { 
      st->SaveINI(settings, path + this->name + "/" + QString::number(index) + "/");
      index ++;
    }
  }
}

#define GET_NODE(X) QJsonValue X##Item::getNode(size_t store)

template<typename B>
inline signed char CompareValue(B a, B b){
  if (a > b){
    return 1;
  } else {
    if (a == b){
      return 0;
    }
    return -1;
  }
}

GET_NODE(jsonStoreList) {
  GET_PTR_TYPE(QJsonStoreListBase)
  QJsonArray array;
  for (auto st : *ptr) {
    if (st != nullptr) {
      array.append(st->ToJson());
    }
  }
  return array;
}

COMPARE(jsonStoreList) {
  GET_PTR_TYPE2(QJsonStoreListBase)
  qsizetype c = a->count();
  qsizetype d = b->count();
  auto r = CompareValue(c, d);
  for (qsizetype i = 0 ; i < c ; i ++){
    if (r != 0){
      return r;
    }
    auto st = a->value(i);
    auto st2 = b->value(i);
    if (st == nullptr){
      return (st2 == nullptr) ? 0 : 1;
    }
    return st->compare(st2);  
  }
  return r;
}

GET_NODE(jsonStore) {
  GET_PTR_TYPE(JsonStore*)
  auto st = *ptr;
  if (st != nullptr) {
    return st->ToJson();
  } else {
    return QJsonValue::Null;
  }
}

COMPARE(jsonStore) {
  GET_PTR_TYPE2(JsonStore*)
  auto st = *a;
  auto st2 = *b;
  if (st == nullptr){
    return (st2 == nullptr) ? 0 : 1;
  }
  return st->compare(st2);  
}


GET_NODE(jsonShared) {
  GET_PTR_TYPE(std::shared_ptr<JsonStore>)
  auto st = *ptr;
  if (st != nullptr) {
    return st->ToJson();
  } else {
    return QJsonValue::Null;
  }
}

COMPARE(jsonShared) {
  GET_PTR_TYPE2(std::shared_ptr<JsonStore>)
  auto st = *a;
  auto st2 = *b;
  if (st == nullptr){
    return (st2 == nullptr) ? 0 : 1;
  }
  return st->compare(st2.get());
}


GET_NODE(enum) {
  GET_PTR_TYPE(std::shared_ptr<JsonEnum>)
  auto st = *ptr;
  if (st != nullptr) {
    return (QString)*st;
  } else {
    return "";
  }
}

COMPARE(enum) {
  GET_PTR_TYPE2(std::shared_ptr<JsonEnum>)
  auto st = *a;
  auto st2 = *b;
  auto r = CompareValue(st == nullptr, st2 == nullptr);
  if (r != 0){
    return r;
  } else if (st == nullptr){
    return 0;
  }
  return CompareValue(st->value, st2->value);
}

GET_NODE(strMap) {
  GET_PTR_TYPE(QVariantMap)
  return QJsonObject::fromVariantMap(*ptr);
}

COMPARE(strMap) {
  GET_PTR_TYPE2(QVariantMap)
  qsizetype c = a->count();
  qsizetype d = b->count();
  auto r = CompareValue(c, d);
  if (r == 0){
    auto keys = a->keys();
    {
      auto other_keys = b->keys();
      r = CompareValue(keys, other_keys);
      if (r != 0){
        return r;
      }
    }
    for (auto u : keys){
      r = CompareValue(a->value(u).toString(), b->value(u).toString());
      if (r != 0){
        return r;
      }
    }
  }
  return r;
}

GET_NODE(intList) {
  GET_PTR_TYPE(QList<int>)
  return QListInt2QJsonArray(*ptr);
}

COMPARE(intList) {
  GET_PTR_TYPE2(QList<int>)
  qsizetype c = a->count();
  qsizetype d = b->count();
  auto r = CompareValue(c, d);
  for (qsizetype i = 0 ; i < c ; i ++){
    if (r != 0){
      return r;
    }
    r = CompareValue(a->value(i), b->value(i));
  }
  return r;
}

GET_NODE(strList) {
  GET_PTR_TYPE(QStringList)
  return QListStr2QJsonArray(*ptr);
}

COMPARE(strList) {
  GET_PTR_TYPE2(QList<QString>)
  qsizetype c = a->count();
  qsizetype d = b->count();
  auto r = CompareValue(c, d);
  for (qsizetype i = 0 ; i < c ; i ++){
    if (r != 0){
      return r;
    }
    r = CompareValue(a->value(i), b->value(i));
  }
  return r;
}

GET_NODE(bool) { 
  GET_PTR_TYPE(bool)
  return *ptr; 
}

COMPARE(bool) {
  GET_PTR_TYPE2(bool)
  return CompareValue(*a, *b);
}


GET_NODE(boolPtr) { 
  GET_PTR_TYPE(bool*)
  return **ptr; 
}

COMPARE(boolPtr) {
  GET_PTR_TYPE2(bool*)
  return CompareValue(**a, **b);
}

GET_NODE(str) { 
  GET_PTR_TYPE(QString)
  return *ptr; 
}

COMPARE(str) {
  GET_PTR_TYPE2(QString)
  return CompareValue(*a, *b);
}

GET_NODE(long) { 
  GET_PTR_TYPE(long long)
  return *ptr; 
}

COMPARE(long) {
  GET_PTR_TYPE2(long long)
  return CompareValue(*a, *b);
}

GET_NODE(int) { 
  GET_PTR_TYPE(int)
  return *ptr; 
}

COMPARE(int) {
  GET_PTR_TYPE2(int)
  return CompareValue(*a, *b);
}

GET_NODE(double) { 
  GET_PTR_TYPE(double)
  return *ptr; 
}

COMPARE(double) {
  GET_PTR_TYPE2(double)
  return CompareValue(*a, *b);
}

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
    CASE_TYPE(enum)
  default:
    return std::make_shared<jsonStoreItem>();
  }
};

int JsonStore::Id() const { return 0; };

QByteArray JsonStore::ToBytes(const QStringList &without, bool header) const {
    QByteArray byteArray;
    QBuffer buffer(&byteArray); // Create a buffer to write to QByteArray
    buffer.open(QIODevice::WriteOnly);
    QDataStream out(&buffer);
    JsonStore * store = (JsonStore*) this; 
    auto _map = store->_map();

    for (auto value: _map.values() ){
      if (value->name == nullptr){
        #ifdef DEBUG_MODE
        qDebug() << "INVALID ITEM ::: UNKNOWN NAME" ;
        #endif
        continue;
      }
        if (value == nullptr){
          #ifdef DEBUG_MODE
          qDebug() << "INVALID ITEM :::" << value->name;
          #endif
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
            bin.store = store;
            out << bin;
        }
    }
    buffer.close();
    if (header){
      return "NekoBox" + byteArray;
    }
    return byteArray;
};


void JsonStore::FromBytes(const QByteArray &data) {
  #ifdef DEBUG_MODE
  qDebug() << "Data Size IS" << data.size() ;
  #endif
  QDataStream stream(data);
  bool fallback = false;
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
    if (value == nullptr || value->type() != type) {
      value = getConfigItem(type);
      store = nullptr;
      fallback = true;
    }
    #ifdef DEBUG_MODE
    if (value == nullptr){
      qDebug() << "SOMETHING STRANGE HERE: JsonStore::FromBytes";
      qDebug() << type;
    }
    #endif
    Bin bin;
    bin.item = value.get();
    bin.store = store;
    stream >> bin;
  }
  if (fallback){
    auto fallback_store = this->fallback();
    if (fallback_store == nullptr){
      return;
    }
    fallback_store->FromBytes(data);
    fallback_store->fallback_job(this);
  }
};

#define PUT_STORE(X)                                                           \
  _put_store(_map, str, value, std::make_shared<X##Item>(), this);

void JsonStore::_put(ConfJsMap _map, const QString &str, int *value)  {
  PUT_STORE(int)
};
void JsonStore::_put(ConfJsMap _map, const QString &str, double *value)  {
  PUT_STORE(double)
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
void JsonStore::_put(ConfJsMap _map, const QString &str,
                     std::shared_ptr<JsonStore>*value) {
  PUT_STORE(jsonShared)
};
void JsonStore::_put(ConfJsMap _map, const QString &str,
                     std::shared_ptr<JsonEnum>*value) {
  PUT_STORE(enum)
};


#define SET_BIN(X)                                                             \
  void X##Item::deserialize(QDataStream &data, size_t store)
#define GET_BIN(X)                                                             \
  void X##Item::serialize(QDataStream &data, size_t store) const

GET_BIN(int) {
  GET_PTR_TYPE(int) 
  data << *ptr; 
}
GET_BIN(double) {   
  GET_PTR_TYPE(double) 
  data << *ptr; 
}
GET_BIN(long) { 
  GET_PTR_TYPE(long long) 
  data << *ptr; 
}
GET_BIN(str) { 
  GET_PTR_TYPE(QString) 
  data << *ptr; 
}
GET_BIN(bool) { 
  GET_PTR_TYPE(bool) 
  data << *ptr; 
}
GET_BIN(boolPtr) { 
  GET_PTR_TYPE(bool*) 
  data << **ptr; 
}
GET_BIN(strList) { 
  GET_PTR_TYPE(QStringList) 
  data << *ptr; 
}
GET_BIN(intList) { 
  GET_PTR_TYPE(QList<int>) 
  data << *ptr; 
}
GET_BIN(strMap) { 
  GET_PTR_TYPE(QVariantMap) 
  data << *ptr; 
}
GET_BIN(jsonStore) {
  GET_PTR_TYPE(JsonStore*) 
  QByteArray array;
  JsonStore *st = *ptr;
  if (st != nullptr) {
    array = st->ToBytes();
  }
  data << array;
}
GET_BIN(jsonShared) {
  QByteArray array;
  GET_PTR_TYPE(std::shared_ptr<JsonStore>) 
  auto st = *ptr;
  if (st != nullptr) {
    array = st->ToBytes();
  }
  data << array;
}
GET_BIN(enum) {
  QByteArray array;
  GET_PTR_TYPE(std::shared_ptr<JsonEnum>) 
  auto st = *ptr;
  if (st != nullptr) {
    array = (QByteArray)*st;
  }
  data << array;
}
GET_BIN(jsonStoreList) {
  QList<QByteArray> value;
  GET_PTR_TYPE(QJsonStoreListBase) 
  for (JsonStore *st : *ptr) {
    value.append(st->ToBytes());
  }
  data << value;
}
SET_BIN(jsonStoreList) {
  QList<QByteArray> value;
  data >> value;
  GET_PTR_OR_RETURN;
  QJsonStoreListBase *base = (QJsonStoreListBase *)ptr;
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
SET_BIN(enum) {
  QByteArray value;
  data >> value;
  GET_PTR_OR_RETURN
  std::shared_ptr<JsonEnum> st = *(std::shared_ptr<JsonEnum> *)ptr;
  if (st != nullptr) {
    *st = value;
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

SET_BIN(double) {
  double value;
  data >> value;
  GET_PTR_OR_RETURN
  *(double *)ptr = value;
}


#include <utility>
#include <functional> // std::hash

// --- Constructors ---
EnumFieldName::EnumFieldName() = default;

EnumFieldName::EnumFieldName(QString n)
    : name(std::move(n))
{}

EnumFieldName::EnumFieldName(std::string n)
    : name(QString::fromUtf8(n.c_str()))
{}


EnumFieldName::EnumFieldName(const char* n)
    : name(QString::fromUtf8(n))
{}

// copy constructor
EnumFieldName::EnumFieldName(EnumFieldName const& other)
    : name(other.name)
{}

// move constructor
EnumFieldName::EnumFieldName(EnumFieldName&& other) noexcept
    : name(std::move(other.name))
{}

// --- Assignment operators ---

// copy assignment
EnumFieldName& EnumFieldName::operator=(EnumFieldName const& other) {
    if (this != &other) {
        name = other.name;
    }
    return *this;
}

// move assignment
EnumFieldName& EnumFieldName::operator=(EnumFieldName&& other) noexcept {
    name = std::move(other.name);
    return *this;
}

// assign from lvalue string
EnumFieldName& EnumFieldName::operator=(const QString & s) {
    name = s;
    return *this;
}

// assign from rvalue string
EnumFieldName& EnumFieldName::operator=(QString&& s) {
    name = std::move(s);
    return *this;
}

// assign from lvalue string
EnumFieldName& EnumFieldName::operator=(const std::string & s) {
    name = QString::fromUtf8(s.c_str());
    return *this;
}

// assign from lvalue string
EnumFieldName& EnumFieldName::operator=(const char* s) {
    name = QString::fromUtf8(s);
    return *this;
}

// assign from rvalue string
EnumFieldName& EnumFieldName::operator=(std::string&& s) {
    name = QString::fromUtf8(std::move(s).c_str());
    return *this;
}

// --- Mutator / setter ---
void EnumFieldName::set_name(QString n) {
    name = std::move(n);
}

// --- Accessors ---
const QString& EnumFieldName::get_name() const noexcept { return name; }

static inline QString ModifyEnumName(QString name){
  name = name.toLower();
  name.replace("_", "");
  name.replace("-", "");
  return name;
}
static inline std::tuple<QString, QString> ModifyEnumName(QString name, QString o_name){
  return std::make_tuple<QString, QString>(ModifyEnumName(name), ModifyEnumName(o_name));
}
// --- Relational operators ---
// Default ordering: case-sensitive on original name. Swap to lower_name if you want case-insensitive ordering.
bool EnumFieldName::operator<(EnumFieldName const& o) const noexcept {
  auto [name, o_name] = ModifyEnumName(this->name, o.name);
  return name < o_name;
}

bool EnumFieldName::operator==(EnumFieldName const& o) const noexcept {  
  auto [name, o_name] = ModifyEnumName(this->name, o.name);
  return name == o_name;
}

bool EnumFieldName::operator!=(EnumFieldName const& o) const noexcept {
    return !(*this == o);
}

bool EnumFieldName::operator>(EnumFieldName const& o) const noexcept {
    return o < *this;
}

bool EnumFieldName::operator<=(EnumFieldName const& o) const noexcept {
    return !(o < *this);
}

bool EnumFieldName::operator>=(EnumFieldName const& o) const noexcept {
    return !(*this < o);
}

// --- Hasher and equality functors ---
// Use lower_name to compute hash and equality (case-insensitive behavior).

std::size_t EnumFieldNameHasher::operator()(EnumFieldName const& w) const noexcept {
  QString key = ModifyEnumName(w.name);
  size_t hash = qHash(key);
  return hash;
}

bool EnumFieldNameEqual::operator()(EnumFieldName const& a, EnumFieldName const& b) const noexcept {
    return a == b;
}

// --- std::hash specialization ---
namespace std {
    std::size_t hash<EnumFieldName>::operator()(EnumFieldName const& w) const noexcept {
        EnumFieldNameHasher hsr;
        return hsr(w);
    }
}

signed char configItem::compare(JsonStore * store, configItem * item, JsonStore * other_store) {
  signed char ret = CompareValue(this->type(), item->type());
  if (ret != 0){
    return ret;
  }
  ret = CompareValue(this->name, item->name);
  if (ret != 0){
    return ret;
  }
  size_t this_ptr = (size_t)this->getPtr(store);
  size_t other_ptr = (size_t)item->getPtr(other_store);
  return this->compare(this_ptr, other_ptr);
}

namespace Configs_ConfigItem {
  signed char JsonStore::compare(JsonStore * store, const QList<QString> &skip){
    if (store == nullptr){
      return 1;
    }
    int ret = 0;
    auto &_map1 = this->_map();
    auto &_map2 = store->_map();
    auto keys = _map1.keys();
    int _map1_size = _map1.count();
    int _map2_size = _map2.count();
    ret = CompareValue(_map1_size, _map2_size);
    if (ret != 0){
      return ret;
    }
    auto keys2 = _map2.keys();
    ret = CompareValue(keys, keys2);
    if (ret != 0){
      return ret;
    }

    for (auto key: keys){
      auto item1 = _map1.find(key);
      auto item2 = _map2.find(key);
      std::shared_ptr<Configs_ConfigItem::configItem> item1val = item1.value();
      std::shared_ptr<Configs_ConfigItem::configItem> item2val = item2.value();
      
      if (item1val == nullptr){
        if (item2val != nullptr){
          return 1;
        }
      } else {
        if (!skip.contains(item1val->name)){
          if (item2val == nullptr){
            return -1;
          } else {
            ret = item1val->compare(this, item2val.get(), store);
            if (ret != 0){
              return ret;
            }
          }
        }
      }
    }
    return 0;
  }
}