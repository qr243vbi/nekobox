#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>

#endif

#pragma once
#ifndef CONFIG_ITEM_H
#define CONFIG_ITEM_H

#include "Configs.hpp"
#include <QJsonObject>
#include <QSettings>
#include <QString>
#include <boost/bimap.hpp>
#include <QFileInfo>


namespace Configs_ConfigItem {
struct configItem;
}

typedef QMap<QByteArray, std::shared_ptr<Configs_ConfigItem::configItem>>
    ConfJsMapStat;
typedef ConfJsMapStat &ConfJsMap;
// inline ConfJsMap initConfJsMap() {
//     return std::make_shared<QMap<QString,
//     std::shared_ptr<Configs_ConfigItem::configItem>>>();
// }

#define GET_FLAG(X, Y) ((X & Y) > 0);
#define SET_FLAG(X, Y, flag)                                                   \
  if (flag) {                                                                  \
    X |= Y;                                                                    \
  } else {                                                                     \
    X &= ~Y;                                                                   \
  }
#define DECLARE_FLAG(X, Y)                                                     \
  bool X() { return GET_FLAG(this->flags, Configs::JsonStoreFlags::Y); }       \
  bool X(bool flag) {                                                          \
    SET_FLAG(this->flags, Configs::JsonStoreFlags::Y, flag);                   \
    return flag;                                                               \
  }

#define DECLARE_STORE_TYPE(X)                                                  \
  virtual char StoreType() const override { return Configs::JsonStoreType::X; };
#define DECLARE_ID_RETURN                                                      \
  virtual int Id() const override { return id; };
#define DECLARE_FLAG_SAME(X) DECLARE_FLAG(X, X)

namespace Configs {
namespace JsonStoreFlags {
const unsigned char save_control_no_save = 0b00000001,
                    storage_exists = 0b00000010, custom_flag2 = 0b01000000,
                    custom_flag = 0b10000000;

};

enum JsonStoreType {
  Routes = 1,
  Proxies = 2,
  Groups = 3,
  Beans = 4,
  Shortcuts = 5,
  ResourceManager = 6,
  ProxyManager = 7,
  NekoBox = 8,
  DefaultRoute = 9,
  NoSave = 10,
  TrafficLooper = 11,
  DatabaseLogger = 12
};

} // namespace Configs

namespace Configs_ConfigItem {

class JsonEnum {
public:
  virtual const boost::bimap<EnumFieldName, int> &_map() const;
  JsonEnum &set(int);
  JsonEnum &set(const QString &);
  JsonEnum &set(const char *);
  JsonEnum &set(const QByteArray &);
  JsonEnum &set(const QJsonValue &);
  operator QJsonValue() const;
  operator int() const;
  operator QString() const;
  operator QByteArray() const;

  template <typename K> JsonEnum &operator=(K val) { return this->set(val); }
  int value;

protected:
  virtual void trigger(int old_value, int new_value);
};

class JsonStore;

class QJsonStoreListBase : public QList<JsonStore *> {
public:
  virtual JsonStore *createJsonStore() = 0;
};

template <typename T = JsonStore>
class QJsonStoreList : public QJsonStoreListBase {
  JsonStore *createJsonStore() override { return new T(); }
};

struct configItem {
  virtual QJsonValue getNode(JsonStore *store) = 0;
  virtual void setNode(JsonStore *store, const QJsonValue &value) = 0;
  virtual void serialize(QDataStream &data, JsonStore *store) const = 0;
  virtual void deserialize(QDataStream &data, JsonStore *store) = 0;
  virtual void SaveINI(JsonStore *store, const QFileInfo& settings, const QString & path) = 0;
  virtual void LoadINI(JsonStore *store, const QFileInfo& settings, const QString & path) = 0;
  virtual unsigned short type() = 0;
  size_t ptr;
  QString name;
  virtual void *getPtr(const JsonStore *store) const;
};

struct Bin {
  configItem *item;
  JsonStore *store;
};

// Serialization function
inline QDataStream &operator<<(QDataStream &out, const Bin &p) {
  p.item->serialize(out, p.store);
  return out;
}

// Deserialization function
inline QDataStream &operator>>(QDataStream &in, Bin &p) {
  p.item->deserialize(in, p.store);
  return in;
}

#define PTR_ITEM(X)                                                            \
  struct X##Item : public configItem {                                         \
    QJsonValue getNode(JsonStore *store) override;                             \
    void setNode(JsonStore *store, const QJsonValue &value) override;          \
    void serialize(QDataStream &data, JsonStore *store) const override;        \
    void deserialize(QDataStream &data, JsonStore *store) override;            \
    void SaveINI(JsonStore *store, const QFileInfo& settings, const QString &path) override;          \
    void LoadINI(JsonStore *store, const QFileInfo& settings, const QString &path) override;          \
    unsigned short type() override { return ConfigItemType::type_##X; };       \
  };

enum ConfigItemType {
  type_int = 0,
  type_long = 1,
  type_str = 2,
  type_bool = 3,
  type_strList = 4,
  type_intList = 5,
  type_jsonStore = 6,
  type_jsonStoreList = 7,
  type_strMap = 8,
  type_boolPtr = 9,
  type_jsonShared = 10,
  type_double = 11,
  type_enum = 12
};

PTR_ITEM(int)
PTR_ITEM(long)
PTR_ITEM(str)
PTR_ITEM(bool)
PTR_ITEM(strList)
PTR_ITEM(intList)
PTR_ITEM(jsonStore)
PTR_ITEM(jsonStoreList)
PTR_ITEM(strMap)
PTR_ITEM(boolPtr)
PTR_ITEM(jsonShared)
PTR_ITEM(double)
PTR_ITEM(enum)

class JsonStore {
private:
  std::shared_ptr<configItem> _get_const_job(const QString &name) const;

public:
  DECLARE_FLAG_SAME(save_control_no_save)
  DECLARE_FLAG_SAME(storage_exists)
  virtual int Id() const;

  QByteArray content(bool is_json);
  bool content(const QByteArray &array);

  virtual ~JsonStore() = default;
  //     QMap<QString, std::shared_ptr<configItem>> _map;

  //    void _put<T>(ConfJsMap _map,
  //        QString str, T * ptr
  //    );
  void _put(ConfJsMap _map, const QString &str, int *);
  void _put(ConfJsMap _map, const QString &str, long long *);
  void _put(ConfJsMap _map, const QString &str, QString *);
  void _put(ConfJsMap _map, const QString &str, bool *);
  void _put(ConfJsMap _map, const QString &str, QStringList *);
  void _put(ConfJsMap _map, const QString &str, QList<int> *);
  void _put(ConfJsMap _map, const QString &str, JsonStore **);
  void _put(ConfJsMap _map, const QString &str, std::shared_ptr<JsonStore> *);
  void _put(ConfJsMap _map, const QString &str, std::shared_ptr<JsonEnum> *);
  void _put(ConfJsMap _map, const QString &str, QVariantMap *);
  void _put(ConfJsMap _map, const QString &str, QJsonStoreListBase *);
  void _put(ConfJsMap _map, const QString &str, bool **);
  void _put(ConfJsMap _map, const QString &str, double *);
  template <typename T, typename = typename std::enable_if<
                            std::is_base_of<JsonStore, T>::value>::type>
  void _put(ConfJsMap _map, const QString &str, T **type) {
    _put(_map, str, (JsonStore **)type);
  }

  template <typename T>
    requires(std::derived_from<T, JsonStore> || std::derived_from<T, JsonEnum>)
  void _put(ConfJsMap _map, const QString &str, std::shared_ptr<T> *type) {
    using Base = std::conditional_t<std::derived_from<T, JsonStore>, JsonStore,
                                    JsonEnum>;
    _put(_map, str, (std::shared_ptr<Base> *)type);
  }
  /*
  template<typename T>
  requires (std::derived_from<T, JsonStore> || std::derived_from<T, JsonEnum>)
  void _put(ConfJsMap map, const QString& str, std::shared_ptr<T>* value)
  {
      using Base = std::conditional_t<
          std::derived_from<T, JsonStore>,
          JsonStore,
          JsonEnum>;

      auto base = std::static_pointer_cast<Base>(*value);
      _put(map, str, &base);
  }
*/
  virtual ConfJsMap _map() = 0;

  //      std::function<void()> callback_after_load = nullptr;
  //      std::function<void()> callback_before_save = nullptr;

  //       QString fn;
  //       bool load_control_must = false;
  //       bool save_control_no_save = false;

  JsonStore() = default;

  //       explicit JsonStore(QString fileName) {
  //           fn = std::move(fileName);
  //       }

  void _setValue(const QString &name, const QJsonValue &p);
  void _setValue(const JsonStore *store, const void *p);
  QJsonValue _getValue(const QString &name) const;

  QString _name(void *p);

  std::shared_ptr<configItem> _get(const QString &name);
  std::shared_ptr<const configItem> _get_const(const QString &name) const;

  QJsonObject ToJson(const QStringList &without = {}) const;

  QByteArray ToJsonBytes(const QStringList &without = {}) const;

  void FromJson(QJsonObject object);

  void LoadINI(const QFileInfo&  settings, const QString &path);

  void SaveINI(const QFileInfo&  settings, const QString &path);

  bool FromJsonBytes(const QByteArray &data);

  void FromBytes(const QByteArray &data);

  QByteArray ToBytes(const QStringList &without = {},
                     bool header = false) const;

  virtual bool Save();

  virtual bool SaveToFile(const QString &file, bool is_json);

  virtual bool Load();

  virtual bool LoadFromFile(const QString &file);

  virtual char StoreType() const = 0;

  virtual bool UnknownKeyHash(const QByteArray &data);

protected:
  unsigned char flags = 0;
};

std::shared_ptr<configItem> getConfigItem(int i);

} // namespace Configs_ConfigItem

using namespace Configs_ConfigItem;

#endif