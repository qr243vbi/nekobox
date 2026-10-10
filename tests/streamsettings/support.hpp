// Test-only boundaries. Production declarations, map macros, field accessors,
// ToJson and FromJson are extracted without changing their bodies by the runner.
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#ifdef USE_REAL_QT
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#else
using QString = std::string;
using QByteArray = std::string;
struct QStringList : std::vector<QString> {
    using std::vector<QString>::vector;
    bool contains(const QString &value) const {
        return std::find(begin(), end(), value) != end();
    }
};
class QJsonValue {
    std::variant<std::monostate, bool, int, QString> value;
public:
    QJsonValue() = default;
    QJsonValue(bool v) : value(v) {}
    QJsonValue(int v) : value(v) {}
    QJsonValue(const QString &v) : value(v) {}
    QJsonValue(const char *v) : value(QString(v)) {}
    bool isBool() const { return std::holds_alternative<bool>(value); }
    bool isString() const { return std::holds_alternative<QString>(value); }
    bool isDouble() const { return std::holds_alternative<int>(value); }
    bool toBool() const { return isBool() ? std::get<bool>(value) : false; }
    QString toString() const { return isString() ? std::get<QString>(value) : ""; }
    int toInt() const { return isDouble() ? std::get<int>(value) : 0; }
};
template<class K, class V> class QMap : public std::map<K, V> {
public:
    std::vector<V> values() const {
        std::vector<V> out;
        for (const auto &[key, value] : *this) { (void)key; out.push_back(value); }
        return out;
    }
    std::vector<K> keys() const {
        std::vector<K> out;
        for (const auto &[key, value] : *this) { (void)value; out.push_back(key); }
        return out;
    }
    V value(const K &key, V fallback = {}) const {
        auto it = this->find(key);
        return it == this->end() ? fallback : it->second;
    }
};
class QJsonObject : public QMap<QString, QJsonValue> {
public:
    void insert(const QString &key, const QJsonValue &value) { (*this)[key] = value; }
    QJsonValue operator[](const QString &key) const { return value(key); }
    QJsonValue &operator[](const QString &key) { return QMap::operator[](key); }
};
#endif

namespace Configs_ConfigItem { class JsonStore; struct configItem; }
using ConfJsMapStat = QMap<QByteArray, std::shared_ptr<Configs_ConfigItem::configItem>>;
using ConfJsMap = ConfJsMapStat &;
namespace Configs {
namespace JsonStoreType { constexpr char NoSave = 10; }
QByteArray hash(const QString &name) {
#ifdef USE_REAL_QT
    return QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Md5);
#else
    return name; // Key identity adapter; collision/hash algorithm is not under test.
#endif
}
}
namespace Configs_ConfigItem {
// The unrelated transport enums are boundary stand-ins in both modes. None of
// the assertions below depends on enum conversion or transport behavior.
class JsonEnum {
    QString text;
public:
    explicit JsonEnum(const char *v) : text(v) {}
    explicit JsonEnum(int) : text("") {}
    operator QString() const { return text; }
    JsonEnum &operator=(const QJsonValue &v) { text = v.toString(); return *this; }
};
struct configItem {
    virtual ~configItem() = default;
    virtual QJsonValue getNode(size_t store) = 0;
    virtual void setNode(size_t store, const QJsonValue &value) = 0;
    size_t ptr;
    QString name;
    void *getPtr(const JsonStore *store) const;
    QJsonValue getNode(const JsonStore *store);
    void setNode(const JsonStore *store, const QJsonValue &value);
};
#define ITEM_DECL(Name) struct Name##Item : configItem { \
    QJsonValue getNode(size_t store) override; \
    void setNode(size_t store, const QJsonValue &value) override; };
ITEM_DECL(int)
ITEM_DECL(str)
ITEM_DECL(bool)
ITEM_DECL(enum)
class JsonStore {
public:
    virtual ~JsonStore() = default;
    virtual char StoreType() const { return Configs::JsonStoreType::NoSave; }
    virtual ConfJsMap _map() = 0;
    virtual std::shared_ptr<JsonStore> fallback() { return nullptr; }
    virtual void fallback_job(JsonStore *) {}
    void _put(ConfJsMap, const QString &, int *);
    void _put(ConfJsMap, const QString &, QString *);
    void _put(ConfJsMap, const QString &, bool *);
    void _put(ConfJsMap, const QString &, std::shared_ptr<JsonEnum> *);
    QJsonObject ToJson(const QStringList &without = {}) const;
    void FromJson(const QJsonObject &object);
};
}
using namespace Configs_ConfigItem;
using V2RAYTransportsEnum = JsonEnum;
using VmessPacketEncodingsEnum = JsonEnum;
#define DECLARE_STORE_TYPE(X) virtual char StoreType() const override { return Configs::JsonStoreType::X; }
