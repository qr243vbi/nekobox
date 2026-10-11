// Test-only Qt/container boundaries. Import and selected Data::Node bodies are
// inserted verbatim by the runner. No application, database, or network is used.
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
#ifdef USE_REAL_QT
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QStringList>
#else
struct QStringList;
struct QString : std::string {
    using std::string::string;
    QString(const std::string &s) : std::string(s) {}
    bool isEmpty() const { return empty(); }
    QString trimmed() const {
        auto first = find_first_not_of(" \t\r\n");
        return first == npos ? QString() : QString(substr(first, find_last_not_of(" \t\r\n") - first + 1));
    }
    QStringList split(const QString &) const;
    static QString number(double n) { std::ostringstream s; s << n; return s.str(); }
};
template<class T> struct QList : std::vector<T> {
    using std::vector<T>::vector;
    size_t count() const { return this->size(); }
    void append(const T &t) { this->push_back(t); }
    QList &operator<<(const T &t) { this->push_back(t); return *this; }
};
struct QStringList : QList<QString> {
    using QList<QString>::QList;
    QString join(const QString &sep) const {
        QString out;
        for (const auto &s : *this) { if (&s != &this->front()) out += sep; out += s; }
        return out;
    }
};
QStringList QString::split(const QString &sep) const {
    QStringList out; size_t start = 0, end;
    while ((end = find(sep, start)) != npos) { out.push_back(substr(start, end - start)); start = end + sep.size(); }
    out.push_back(substr(start)); return out;
}
template<class K, class V> struct QMap : std::map<K,V> {
    using std::map<K,V>::map;
    QList<V> values() const { QList<V> out; for (auto &entry : *this) out.push_back(entry.second); return out; }
    size_t count() const { return this->size(); }
};
class QJsonObject;
class QJsonArray;
struct QJsonValue {
    std::variant<std::monostate, bool, double, QString, std::shared_ptr<QJsonObject>, std::shared_ptr<QJsonArray>> data;
    QJsonValue() = default;
    QJsonValue(bool v) : data(v) {}
    QJsonValue(int v) : data(double(v)) {}
    QJsonValue(double v) : data(v) {}
    QJsonValue(const char *s) : data(QString(s)) {}
    QJsonValue(const QString &s) : data(s) {}
    QJsonValue(const QJsonObject &v);
    QJsonValue(const QJsonArray &v);
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isDouble() const { return std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<QString>(data); }
    bool isObject() const { return std::holds_alternative<std::shared_ptr<QJsonObject>>(data); }
    bool isArray() const { return std::holds_alternative<std::shared_ptr<QJsonArray>>(data); }
    bool isNull() const { return std::holds_alternative<std::monostate>(data); }
    int type() const { return 0; }
    bool toBool() const { return isBool() && std::get<bool>(data); }
    double toDouble() const { return isDouble() ? std::get<double>(data) : 0; }
    QString toString() const { return isString() ? std::get<QString>(data) : QString(); }
    QJsonObject toObject() const;
    QJsonArray toArray() const;
};
struct QJsonObject : QMap<QString,QJsonValue> {
    using QMap<QString,QJsonValue>::QMap;
    bool isEmpty() const { return empty(); }
    QJsonValue operator[](const QString &key) const { auto i = find(key); return i == end() ? QJsonValue() : i->second; }
    QJsonValue &operator[](const QString &key) { return QMap::operator[](key); }
};
struct QJsonArray : QList<QJsonValue> { using QList<QJsonValue>::QList; };
QJsonValue::QJsonValue(const QJsonObject &v) : data(std::make_shared<QJsonObject>(v)) {}
QJsonValue::QJsonValue(const QJsonArray &v) : data(std::make_shared<QJsonArray>(v)) {}
QJsonObject QJsonValue::toObject() const { return isObject() ? *std::get<std::shared_ptr<QJsonObject>>(data) : QJsonObject(); }
QJsonArray QJsonValue::toArray() const { return isArray() ? *std::get<std::shared_ptr<QJsonArray>>(data) : QJsonArray(); }
#endif

struct EnumFieldName : QString {
    using QString::QString;
    EnumFieldName(const QString &s) : QString(s) {}
    QString get_name() const { return *this; }
};
namespace Configs { namespace Data {
enum class Tag { True, False, Map, Array, String, Number, Null, Undefined };
class Node;
using Value = std::variant<QMap<EnumFieldName,Node>,QList<Node>,QString,long double,bool>;
class Node {
    Tag tag;
    Value value;
public:
    explicit Node(Tag t = Tag::Undefined) : tag(t) {
        switch (tag) {
            case Tag::Map: value = QMap<EnumFieldName,Node>(); break;
            case Tag::Array: value = QList<Node>(); break;
            case Tag::String: value = QString(); break;
            case Tag::Number: value = static_cast<long double>(0); break;
            default: value = false;
        }
    }
    explicit Node(const QJsonValue &v) : Node() {
        if (v.isBool()) { tag = v.toBool() ? Tag::True : Tag::False; }
        else if (v.isDouble()) { tag = Tag::Number; value = static_cast<long double>(v.toDouble()); }
        else if (v.isString()) { tag = Tag::String; value = v.toString(); }
        else if (v.isArray()) {
            tag = Tag::Array; QList<Node> list;
            for (auto item : v.toArray()) list.push_back(Node(item));
            value = list;
        } else if (v.isObject()) {
            tag = Tag::Map; QMap<EnumFieldName,Node> map;
            auto obj = v.toObject();
#ifdef USE_REAL_QT
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) map[EnumFieldName(it.key())] = Node(it.value());
#else
            for (auto &entry : obj) map[EnumFieldName(entry.first)] = Node(entry.second);
#endif
            value = map;
        } else { tag = Tag::Null; }
    }
    Node(const QJsonObject &obj) : Node(QJsonValue(obj)) {}
    int toInt() const { return 0; } // Unrelated bean numeric fields are not under test.
    int toVariantMap() const { return 0; } // Unrelated headers are not under test.
    bool isMap() const; bool isObject() const; bool isArray() const;
    bool isString() const; bool isNumber() const; bool isBoolean() const;
    bool isBool() const; bool isNull() const; bool isUndefined() const; bool isNothing() const;
    bool toBoolean() const; bool toBool() const; bool getBoolean(bool def = false) const;
    QString getString(const EnumFieldName &def = "") const;
    QString toString() const; QStringList toStringList() const;
    size_t count() const; QList<Node> values() const;
    bool contains(const EnumFieldName &) const;
    // Container lookups and object construction are explicit test adapters.
    const Node &operator[](const EnumFieldName &key) const {
        static Node undefined;
        if (!isMap()) return undefined;
        const auto &map = std::get<QMap<EnumFieldName,Node>>(value);
        auto i = map.find(key);
#ifdef USE_REAL_QT
        return i == map.end() ? undefined : i.value();
#else
        return i == map.end() ? undefined : i->second;
#endif
    }
    QString toQuoted() const { return "<unexercised composite string conversion>"; }
};
} }
QJsonArray QListStr2QJsonArray(const QStringList &list) { QJsonArray out; for (const auto &s : list) out.push_back(s); return out; }
template<class T> void add_non_empty(T &obj, const QString &key, const QString &value) { if (!value.isEmpty()) obj[key] = value; }
namespace Configs {
struct V2rayStreamSettings {
    // ECH declarations are inserted from the production header below.
ECH_DECLARATIONS
    std::shared_ptr<QString> packet_encoding = std::make_shared<QString>();
    QString security, reality_pbk, reality_sid, utlsFingerprint, tls_fragment_fallback_delay, sni, alpn;
    bool enable_tls_fragment = false, enable_tls_record_fragment = false, allow_insecure = false;
    QJsonObject exportEch() const;
};
namespace From_Json { bool add_tls(std::shared_ptr<V2rayStreamSettings>, const Data::Node &); }
}

namespace Configs {
struct ProxyEntity;
struct AbstractBean {
    virtual ~AbstractBean() = default;
    virtual bool TryParseJson(const Data::Node &) = 0;
    ProxyEntity *entity = nullptr;
    std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();
    QString password, username, uuid, path, flow, encryption, security, idle_session_check_interval, idle_session_timeout;
    int min_idle_session = 0, insecure_concurrency = 0, shadowtls_version = 0, aid = 0, headers = 0, extra_headers = 0;
    bool health_check = false, global_padding = false, authenticated_length = false;
    enum { proxy_Trojan, proxy_VLESS } proxy_type = proxy_VLESS;
};
#define BEAN(Name) struct Name##Bean : AbstractBean { bool TryParseJson(const Data::Node &) override; };
BEAN(AnyTLS) BEAN(Http) BEAN(Juicity) BEAN(Naive) BEAN(ShadowTLS) BEAN(TrojanVLESS) BEAN(VMess)
struct TrustTunnelBean : AbstractBean { bool TryParseJson(const Data::Node &) override; bool TryParseYaml(const Data::Node &); };
#undef BEAN
namespace From_Json {
// These unrelated bean services are boundaries; only add_tls is exercised.
void add_default_fields(ProxyEntity *, const Data::Node &) {}
template<class T> void add_username_password(T *, const Data::Node &) {}
template<class T> void add_udp_over_tcp(T *, const Data::Node &) {}
template<class T> void add_quic(T *, const Data::Node &) {}
template<class T> void add_network(T *, const Data::Node &) {}
void add_mux_state(AbstractBean *, const Data::Node &) {}
bool parse_transport(std::shared_ptr<V2rayStreamSettings>, const Data::Node &) { return false; }
}
struct ProxyEntity {
    QString name;
    std::shared_ptr<AbstractBean> storage;
    bool isValid() const { return storage != nullptr; }
    auto bean() { return storage; }
    template<class T> auto unlock(T value) { return value; }
};
struct ProfileManager {
    static auto NewProxyEntity(const QString &type, bool) {
        auto ent = std::make_shared<ProxyEntity>();
        if (type == "anytls") ent->storage = std::make_shared<AnyTLSBean>();
        else if (type == "http") ent->storage = std::make_shared<HttpBean>();
        else if (type == "juicity") ent->storage = std::make_shared<JuicityBean>();
        else if (type == "naive") ent->storage = std::make_shared<NaiveBean>();
        else if (type == "shadowtls") ent->storage = std::make_shared<ShadowTLSBean>();
        else if (type == "trojan" || type == "vless") ent->storage = std::make_shared<TrojanVLESSBean>();
        else if (type == "trusttunnel") ent->storage = std::make_shared<TrustTunnelBean>();
        else if (type == "vmess") ent->storage = std::make_shared<VMessBean>();
        if (ent->storage) ent->storage->entity = ent.get();
        return ent;
    }
};
}
QString QJsonType2QString(int) { return "fixture type"; }
void MW_show_log(const QString &) {}
QJsonObject convertV2RayNToSingBox(const QJsonObject &obj) { return obj; }
namespace Subscription {
int fixed_entities = 0;
void RawUpdater_FixEnt(const std::shared_ptr<Configs::ProxyEntity> &) { ++fixed_entities; }
struct RawUpdater {
    QList<std::shared_ptr<Configs::ProxyEntity>> proxies;
    void updateSingBox(const QJsonObject &, const QString &);
    void AddProxy(const std::shared_ptr<Configs::ProxyEntity> &ent) { proxies.push_back(ent); }
};
}
