// Test-only dependency boundaries for the verbatim RawUpdater::update body.
// In adapter mode Python pre-parses explicit fixtures; this is NOT a Qt parser.
#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#ifdef USE_REAL_QT
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QUrl>
#else
struct QChar {
    char value;
    operator char() const { return value; }
    unsigned short unicode() const { return static_cast<unsigned char>(value); }
    static bool isLetterOrNumber(unsigned short v) { return std::isalnum(v); }
};
struct QString : std::string {
    using std::string::string;
    QString(const std::string &v) : std::string(v) {}
    bool isEmpty() const { return empty(); }
    bool startsWith(const char *prefix) const { return starts_with(prefix); }
    QString toUtf8() const { return *this; }
    QString toLower() const { auto s = *this; std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); }); return s; }
    QString toUpper() const { auto s = *this; std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); }); return s; }
    QString sliced(int start, int count) const { return substr(start, count); }
    QString mid(int start, int count) const { return substr(start, count); }
    int count(const char *s) const { return static_cast<int>(std::count(begin(), end(), s[0])); }
    int indexOf(char c, int start) const { auto i = find(c, start); return i == npos ? -1 : static_cast<int>(i); }
    QString trimmed() const {
        auto first = find_first_not_of(" \t\r\n");
        if (first == npos) return {};
        return substr(first, find_last_not_of(" \t\r\n") - first + 1);
    }
    QChar operator[](int n) const { return {std::string::operator[](n)}; }
};
using QByteArray = QString;
template<class T> struct QList : std::vector<T> {
    using std::vector<T>::vector;
    void append(const T &v) { this->push_back(v); }
    QList &operator<<(const T &v) { this->push_back(v); return *this; }
    T takeFirst() { auto v = this->front(); this->erase(this->begin()); return v; }
    bool isEmpty() const { return this->empty(); }
};
template<class K, class V> struct QMap : std::map<K,V> { using std::map<K,V>::map; };
class QJsonObject;
class QJsonArray;
class QJsonValue {
public:
    std::variant<std::monostate, bool, int, QString, std::shared_ptr<QJsonObject>, std::shared_ptr<QJsonArray>> data;
    QJsonValue() = default;
    QJsonValue(bool v) : data(v) {}
    QJsonValue(int v) : data(v) {}
    QJsonValue(const char *v) : data(QString(v)) {}
    QJsonValue(const QString &v) : data(v) {}
    QJsonValue(const QJsonObject &v);
    QJsonValue(const QJsonArray &v);
    bool isString() const { return std::holds_alternative<QString>(data); }
    bool isObject() const { return std::holds_alternative<std::shared_ptr<QJsonObject>>(data); }
    bool isArray() const { return std::holds_alternative<std::shared_ptr<QJsonArray>>(data); }
    QString toString() const { return isString() ? std::get<QString>(data) : QString(); }
    int toInt() const { return std::holds_alternative<int>(data) ? std::get<int>(data) : 0; }
    QJsonObject toObject() const;
    QJsonArray toArray() const;
};
class QJsonObject : public QMap<QString,QJsonValue> {
public:
    using QMap<QString,QJsonValue>::QMap;
    QJsonValue operator[](const QString &s) const { auto i = find(s); return i == end() ? QJsonValue() : i->second; }
    QJsonValue &operator[](const QString &s) { return QMap::operator[](s); }
};
class QJsonArray : public QList<QJsonValue> { public: using QList<QJsonValue>::QList; };
QJsonValue::QJsonValue(const QJsonObject &v) : data(std::make_shared<QJsonObject>(v)) {}
QJsonValue::QJsonValue(const QJsonArray &v) : data(std::make_shared<QJsonArray>(v)) {}
QJsonObject QJsonValue::toObject() const { return isObject() ? *std::get<std::shared_ptr<QJsonObject>>(data) : QJsonObject(); }
QJsonArray QJsonValue::toArray() const { return isArray() ? *std::get<std::shared_ptr<QJsonArray>>(data) : QJsonArray(); }
struct QJsonParseError { enum { NoError, IllegalValue }; int error = NoError; };
std::map<QString,QJsonValue> jsonFixtures;
class QJsonDocument {
    QJsonValue root;
public:
    enum { Compact };
    QJsonDocument() = default;
    QJsonDocument(const QJsonObject &obj) : root(obj) {}
    bool isArray() const { return root.isArray(); }
    bool isObject() const { return root.isObject(); }
    QJsonArray array() const { return root.toArray(); }
    QJsonObject object() const { return root.toObject(); }
    QByteArray toJson(int) const { return "{\"adapter_full_config\":true}"; } // Serialization is not under test.
    static QJsonDocument fromJson(const QByteArray &str, QJsonParseError *err = nullptr) {
        auto it = jsonFixtures.find(str);
        if (it == jsonFixtures.end()) throw std::runtime_error("Unregistered JSON parser boundary: " + str);
        QJsonDocument doc; doc.root = it->second;
        if (err) err->error = doc.isObject() || doc.isArray() ? QJsonParseError::NoError : QJsonParseError::IllegalValue;
        return doc;
    }
};
struct QUrl {
    QString text;
    explicit QUrl(const QString &s) : text(s) {}
    bool isValid() const { return text == "nekoray://custom/fixture"; }
    QString host() const { return "custom"; }
};
#endif

namespace Configs {
struct CoreObjOutboundBuildResult { QJsonObject outbound; };
struct AbstractBean {
    virtual ~AbstractBean() = default;
    bool TryParseLink(const QString &s) { return s == "socks://127.0.0.1:1080" || s == "SOCKS://127.0.0.1:1080"; }
    bool TryParseNekorayLink(const QUrl &url) { return url.isValid(); }
};
struct CustomBean : AbstractBean {
    QString core, config_simple;
    CoreObjOutboundBuildResult BuildCoreObjSingBox() const;
};
struct ProxyEntity {
    QString type, name, display_type;
    std::shared_ptr<Configs::CustomBean> storage = std::make_shared<Configs::CustomBean>();
    explicit ProxyEntity(QString t) : type(std::move(t)) {}
    bool isValid() const { return type == "custom" || type == "socks"; }
    auto CustomBean() { return storage; }
    auto bean() { return storage; }
    template<class T> auto unlock(T v) { return v; }
};
struct ProfileManager {
    static auto NewProxyEntity(const QString &type, bool = false) { return std::make_shared<ProxyEntity>(type); }
};
struct ProfileFilterKey {
    QString key;
    ProfileFilterKey(const std::shared_ptr<ProxyEntity> &ent, bool) : key(ent->type + ":" + ent->storage->config_simple) {}
    bool operator<(const ProfileFilterKey &other) const { return key < other.key; }
};
}
QJsonObject QString2QJsonObject(const QString &text) { return QJsonDocument::fromJson(text.toUtf8()).object(); }
QString DecodeB64IfValid(const QString &) { return {}; } // Base64 codec not under test.
struct QObject { static QString tr(const char *text) { return text; } };
void MW_show_log(const QString &) {}
namespace HappDecrypt { QString decryptLink(const QString &) { return {}; } }
int sanitizeCalls = 0, fixCalls = 0;
QJsonObject sanitizeSingBoxConfig(const QJsonObject &obj) { ++sanitizeCalls; return obj; }
namespace Subscription {
void RawUpdater_FixEnt(const std::shared_ptr<Configs::ProxyEntity> &) { ++fixCalls; }
struct RawUpdater {
    QList<std::shared_ptr<Configs::ProxyEntity>> proxies;
    QMap<Configs::ProfileFilterKey,bool> ignore_map;
    QList<QJsonObject> envelopes;
    QList<QString> envelopeNames;
    int sipCalls = 0;
    void update(const QString &);
    bool AddProxy(std::shared_ptr<Configs::ProxyEntity>);
    void updateSingBox(const QJsonObject &obj, const QString &name = {}) { envelopes << obj; envelopeNames << name; }
    void updateSIP008(const QJsonObject &) { ++sipCalls; }
    bool updateClash(const QString &) { return false; }
    bool updateWireguardFileConfig(const QString &) { return false; }
};
}
