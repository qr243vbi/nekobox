#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <nekobox/dataStore/ConfigItem.hpp>
#include "libcore_types.h"
#include <nekobox/dataStore/Configs.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <qbuffer.h>
#include <qcborcommon.h>
#include <qcontainerfwd.h>

#include <boost/algorithm/string.hpp>
#include <qnamespace.h>


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
  if (value.isArray()) {
    QJsonArray array = value.toArray();
    {
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


#define LOAD_CONF(X) void X##Item::LoadINI(JsonStore * store, const QFileInfo& settings, const QString &path)

LOAD_CONF(int) {
  auto val =  QSettingsFromFileInfo(settings);
  int * ptr = (int *)getPtr(store);
  *ptr = val.value(path + this->name, 0).toInt();
}

LOAD_CONF(double) {
  auto val =  QSettingsFromFileInfo(settings);
  double * ptr = (double *)getPtr(store);
  *ptr = val.value( path + this->name, 0).toDouble();
}

LOAD_CONF(long) {
  auto val =  QSettingsFromFileInfo(settings);
  long long * ptr = (long long *)getPtr(store);
  *ptr = val.value( path + this->name, 0).toLongLong();
}

LOAD_CONF(bool) {
  auto val =  QSettingsFromFileInfo(settings);
  bool * ptr = (bool *)getPtr(store);
  *ptr = val.value( path + this->name, false).toBool();
}

LOAD_CONF(boolPtr) {
  auto val =  QSettingsFromFileInfo(settings);
  bool ** ptr = (bool **)getPtr(store);
  **ptr = val.value( path + this->name, false).toBool();
}

LOAD_CONF(str) {
  auto val =  QSettingsFromFileInfo(settings);
  QString * ptr = (QString *)getPtr(store);
  *ptr = val.value( path + this->name, "").toString();
}

LOAD_CONF(strList) {
  auto val =  QSettingsFromFileInfo(settings);
  QList<QString> * ptr = (QList<QString> *)getPtr(store);
  *ptr = val.value( path + this->name, {}).toStringList();
}

LOAD_CONF(intList) {
  auto val =  QSettingsFromFileInfo(settings);
  QList<int> * ptr = (QList<int> *)getPtr(store);
  *ptr = QListStr2QListInt(val.value(path + this->name, {}).toStringList());
}

LOAD_CONF(strMap) {
  QVariantMap * ptr = (QVariantMap *)getPtr(store);
  QSettings ini = QSettingsFromFileInfo(settings);
  ptr->clear();
  ini.beginGroup(path + this->name);
  for (auto & key : ini.childKeys()){
    ptr->insert(key, ini.value(key, ""));
  }
  ini.endGroup();
}

LOAD_CONF(enum) {
  auto val =  QSettingsFromFileInfo(settings);
  std::shared_ptr<JsonEnum> st = *(std::shared_ptr<JsonEnum> *)(this->getPtr(store));
  if (st != nullptr) {
    st->set(val.value(path + this->name, "").toString());
  }
}

LOAD_CONF(jsonShared) {
  std::shared_ptr<JsonStore> st = *(std::shared_ptr<JsonStore> *)(this->getPtr(store));
  if (st != nullptr) {
    st->LoadINI(settings, path + this->name + "/");
  }
}
LOAD_CONF(jsonStore) {
  JsonStore *st = *(JsonStore **)(this->getPtr(store));
  if (st != nullptr) {
    st->LoadINI(settings, path + this->name + "/");
  }
}
LOAD_CONF(jsonStoreList) {
  auto list = (QJsonStoreListBase *)this->getPtr(store);
  int index = 0; if (list == nullptr) return;
  QStringList keys;
  {
    auto val = QSettingsFromFileInfo(settings);
    val.beginGroup(path + this->name);
    keys = val.childGroups();
    val.endGroup();
  }
  index = keys.count();
  for (int i = 0; i < index; i ++){
    list->append(list->createJsonStore());
  }

  for (auto st : keys) {
    {
      bool ok;
      int key = st.toInt(&ok);
      if (ok){
        auto stt = list->value(key, nullptr) ;
        if (stt != nullptr) {
          stt->LoadINI(settings, path + this->name + "/" + st + "/");
        }
      } 
    }
  }
}

#define SAVE_CONF(X) void X##Item::SaveINI(JsonStore * store, const QFileInfo& settings, const QString &path)

SAVE_CONF(int) {
  auto val = QSettingsFromFileInfo(settings);
  auto * ptr = (int *)getPtr(store);
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(double) {
  auto val = QSettingsFromFileInfo(settings);
  auto * ptr = (double *)getPtr(store);
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(long) {
  auto val = QSettingsFromFileInfo(settings);
  auto * ptr = (long long *)getPtr(store);
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(bool) {
  auto val = QSettingsFromFileInfo(settings);
  auto * ptr = (bool *)getPtr(store);
  val.setValue(path + this->name, *ptr);
}

SAVE_CONF(boolPtr) {
  auto val = QSettingsFromFileInfo(settings);
  auto * ptr = (bool * *)getPtr(store);
  val.setValue(path + this->name, **ptr);
}

SAVE_CONF(str) {
  auto val = QSettingsFromFileInfo(settings);
  auto ptr = *(QString *)getPtr(store);
  val.setValue(path + this->name, ptr);
}

SAVE_CONF(strList) {
  auto val = QSettingsFromFileInfo(settings);
  auto ptr = *(QStringList *)getPtr(store);
  val.setValue(path + this->name, ptr);
}

SAVE_CONF(intList) {
  auto val = QSettingsFromFileInfo(settings);
  auto * ptr = (QList<int> *)getPtr(store);
  val.setValue(path + this->name, QListInt2QListStr(*ptr));
}

SAVE_CONF(strMap) {
  QVariantMap * ptr = (QVariantMap *)getPtr(store);
  QSettings ini = QSettingsFromFileInfo(settings);
  ini.beginGroup(path + this->name);
  ini.remove("");
  for (auto [key, value] : asKeyValueRange(*ptr)){
    ini.setValue(key, value);
  }
  ini.endGroup();
}

SAVE_CONF(enum) {
  std::shared_ptr<JsonEnum> st = *(std::shared_ptr<JsonEnum> *)(this->getPtr(store));
  if (st != nullptr) {
    auto ini = QSettingsFromFileInfo(settings);
    ini.setValue(path + this->name, (QString)*st);
  }
}

SAVE_CONF(jsonShared) {
  std::shared_ptr<JsonStore> st = *(std::shared_ptr<JsonStore> *)(this->getPtr(store));
  if (st != nullptr) {
    st->SaveINI(settings, path + this->name + "/");
  }
}
SAVE_CONF(jsonStore) {
  JsonStore *st = *(JsonStore **)(this->getPtr(store));
  if (st != nullptr) {
    st->SaveINI(settings, path + this->name + "/");
  }
}
SAVE_CONF(jsonStoreList) {
  auto list = (QJsonStoreListBase *)this->getPtr(store);
  int index = 0; if (list == nullptr) return;
  for (auto st : *list) {
    if (st != nullptr) { 
      st->SaveINI(settings, path + this->name + "/" + QString::number(index) + "/");
      index ++;
    }
  }
}

#define GET_NODE(X) QJsonValue X##Item::getNode(JsonStore *store)

GET_NODE(jsonStoreList) {
  auto list = (QJsonStoreListBase *)this->getPtr(store);
  QJsonArray array; if (list == nullptr) return array;
  for (auto st : *list) {
    if (st != nullptr) {
      array.append(st->ToJson());
    }
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
GET_NODE(enum) {
  std::shared_ptr<JsonEnum> st = *(std::shared_ptr<JsonEnum> *)(this->getPtr(store));
  if (st != nullptr) {
    return (QString)*st;
  } else {
    return "";
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

GET_NODE(double) { return *(double *)getPtr(store); }


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

bool JsonStore::UnknownKeyHash(const QByteArray &data){
    return false;
}

void JsonStore::FromBytes(const QByteArray &data) {
  #ifdef DEBUG_MODE
  qDebug() << "Data Size IS" << data.size() ;
  #endif
  QDataStream stream(data);
  auto _map = this->_map();
  while (!stream.atEnd()) {
    QByteArray key;
    stream >> key;
    unsigned char type;
    stream >> type;
    auto iter = _map.find(key);
    bool cont = false;
    std::shared_ptr<configItem> value = nullptr;
    JsonStore *store = this;
    if (iter != _map.end()) {
      value = iter.value();
    } else {
        cont = UnknownKeyHash(key);
    }
    if (cont && (type == ConfigItemType::type_jsonStore || type == ConfigItemType::type_jsonShared)){
        QByteArray array;
        stream >> array;
        this->FromBytes(array);
        continue;
    }
    if (value == nullptr || value->type() != type) {
      value = getConfigItem(type);
      store = nullptr;
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
  void X##Item::deserialize(QDataStream &data, JsonStore *store)
#define GET_BIN(X)                                                             \
  void X##Item::serialize(QDataStream &data, JsonStore *store) const

GET_BIN(int) { data << *(int *)this->getPtr(store); }
GET_BIN(double) { data << *(double *)this->getPtr(store); }
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
GET_BIN(enum) {
  QByteArray array;
  std::shared_ptr<JsonEnum> st = *(std::shared_ptr<JsonEnum>*)this->getPtr(store);
  if (st != nullptr) {
    array = (QByteArray)*st;
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













#include <algorithm> // std::transform
#include <cctype>    // std::tolower
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

// --- Relational operators ---
// Default ordering: case-sensitive on original name. Swap to lower_name if you want case-insensitive ordering.
bool EnumFieldName::operator<(EnumFieldName const& o) const noexcept {
    return QString::compare(name, o.name, Qt::CaseInsensitive) < 0;
}

bool EnumFieldName::operator==(EnumFieldName const& o) const noexcept {
    return QString::compare(name, o.name, Qt::CaseInsensitive) == 0;
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

// Comparisons with std::string (case-sensitive against original name)
bool EnumFieldName::operator==(QString const& s) const noexcept {
    return QString::compare(name, s, Qt::CaseInsensitive) == 0;
}

bool EnumFieldName::operator!=(const QString& s) const noexcept {
    return !(*this == s);
}

// --- Hasher and equality functors ---
// Use lower_name to compute hash and equality (case-insensitive behavior).

std::size_t EnumFieldNameHasher::operator()(EnumFieldName const& w) const noexcept {
  QString key = w.name;  // best option
  key = key.toCaseFolded();
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
