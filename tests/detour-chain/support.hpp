// Test-only profile, bean and storage boundaries. Strings, JSON and containers
// are real Qt6; the importer, chain resolver and profile key code are extracted
// from the production sources. No application, core, profile storage or network
// is used.
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <initializer_list>
#include <set>
#include <utility>

QStringList logLines;
inline std::function<void(const QString &)> MW_show_log = [](const QString &m) { logLines << m; };
QString QJsonType2QString(QJsonValue::Type) { return "other"; }
#ifdef REAL_IMPORT_HELPERS
// the whole import path is extracted: base64, splitter, sanitizer, dispatch
QByteArray DecodeB64IfValid(const QString &input,
                            QByteArray::Base64Options options = QByteArray::Base64Encoding);
#else
// v2rayN conversion is not under test: the fixtures are already sing-box.
QJsonObject convertV2RayNToSingBox(const QJsonObject &o) { return o; }
#endif
namespace HappDecrypt { QString decryptLink(const QString &) { return {}; } }

namespace Configs {

struct ProxyEntity;

// Stands in for every real bean: it keeps the parsed JSON, which is also what
// two beans are compared by.
struct AbstractBean {
    ProxyEntity *entity;
    QJsonObject json;
    explicit AbstractBean(ProxyEntity *e) : entity(e) {}
    virtual ~AbstractBean() = default;
    virtual bool TryParseJson(const QJsonObject &obj);
    // share links: only what tells two profiles apart, the real parsers are
    // covered by their own tests
    virtual bool TryParseLink(const QString &link);
    bool TryParseNekorayLink(const QUrl &) { return false; }
    virtual QString fingerprint() const { return QJsonDocument(json).toJson(QJsonDocument::Compact); }
    int compare(const AbstractBean *other, const QStringList &) const {
        return fingerprint().compare(other->fingerprint());
    }
};

struct ChainBean : AbstractBean {
    QList<int> list; // in to out
    QString import_key;
    using AbstractBean::AbstractBean;
    QString fingerprint() const override {
        QStringList parts;
        for (int id : list) parts << QString::number(id);
        return "chain:" + parts.join(",") + ":" + import_key;
    }
};

struct CustomBean : AbstractBean {
    QString core, config_simple;
    using AbstractBean::AbstractBean;
    QString fingerprint() const override { return "custom:" + core + ":" + config_simple; }
};

struct ShadowSocksBean : AbstractBean {
    using AbstractBean::AbstractBean;
    bool TryParseFromSIP008(const QJsonObject &obj);
};

struct ProxyEntity {
    QString type, display_type, name;
    QString serverAddress = "127.0.0.1";
    int serverPort = 1080;
    int id = -1;
    int gid = 0;
    bool chain_link = false;
    int saves = 0;
    std::shared_ptr<AbstractBean> storage;
    explicit ProxyEntity(QString t) : type(std::move(t)) {
        if (type == "chain") storage = std::make_shared<Configs::ChainBean>(this);
        else if (type == "custom") storage = std::make_shared<Configs::CustomBean>(this);
        else if (type == "shadowsocks") storage = std::make_shared<Configs::ShadowSocksBean>(this);
        else storage = std::make_shared<AbstractBean>(this);
    }
    bool isValid() const {
        // direct, block, selector... are not profiles
        return type == "vless" || type == "ssh" || type == "trojan" || type == "chain" ||
               type == "custom" || type == "shadowsocks";
    }
    std::shared_ptr<const AbstractBean> bean() const { return storage; }
    std::shared_ptr<const Configs::ChainBean> ChainBean() const {
        return std::dynamic_pointer_cast<const Configs::ChainBean>(storage);
    }
    std::shared_ptr<const Configs::CustomBean> CustomBean() const {
        return std::dynamic_pointer_cast<const Configs::CustomBean>(storage);
    }
    std::shared_ptr<const Configs::ShadowSocksBean> ShadowSocksBean() const {
        return std::dynamic_pointer_cast<const Configs::ShadowSocksBean>(storage);
    }
    bool Save() { ++saves; return true; }
    int Id() const { return id; }
    template <class A> std::shared_ptr<A> unlock(std::shared_ptr<const A> b) {
        return std::const_pointer_cast<A>(b);
    }
};

bool AbstractBean::TryParseJson(const QJsonObject &obj) {
    if (obj["unparsable"].toBool()) return false;
    json = obj;
    entity->name = obj["tag"].toString();
    entity->serverAddress = obj["server"].toString();
    entity->serverPort = obj["server_port"].toInt();
    return true;
}

bool AbstractBean::TryParseLink(const QString &link) {
    QUrl url(link);
    if (!url.isValid() || url.host().isEmpty()) return false;
    json = QJsonObject{{"link", link}};
    entity->name = url.fragment(QUrl::FullyDecoded);
    entity->serverAddress = url.host();
    entity->serverPort = url.port(443);
    return true;
}

bool ShadowSocksBean::TryParseFromSIP008(const QJsonObject &obj) {
    json = obj;
    entity->name = obj["remarks"].toString();
    entity->serverAddress = obj["server"].toString();
    entity->serverPort = obj["server_port"].toInt();
    return true;
}

PROFILE_FILTER_KEY_DECL

struct Group {
    int id = 1;
    QList<int> profiles;
    int saves = 0;
    void Save() { ++saves; }
};

struct DataStore {
    int current_group = 1;
    int core_box_clash_api = 0;
    QString core_box_clash_listen_addr = "127.0.0.1";
    QString core_box_clash_api_secret;
} dataStoreValue;
DataStore *dataStore = &dataStoreValue;

// Same observable contract as ProfileManager: AddProfileBatch gives ids in
// order and appends to the group, ReplaceProfile keeps the id of the old one.
struct ProfileManager {
    std::map<int, std::shared_ptr<ProxyEntity>> ents;
    std::map<int, std::shared_ptr<Group>> groups;
    int maxId = 0;
    int lockDepth = 0;
    int maxLockDepth = 0;
    static std::shared_ptr<ProxyEntity> NewProxyEntity(QString type, bool = false) {
        return std::make_shared<ProxyEntity>(type);
    }
    std::shared_ptr<Group> GetGroup(int id) {
        auto it = groups.find(id);
        return it == groups.end() ? nullptr : it->second;
    }
    std::shared_ptr<ProxyEntity> GetProfile(int id) {
        auto it = ents.find(id);
        return it == ents.end() ? nullptr : it->second;
    }
    bool AddProfileBatch(const QList<std::shared_ptr<ProxyEntity>> &list, int gid = -1) {
        gid = gid < 0 ? dataStore->current_group : gid;
        auto group = GetGroup(gid);
        if (group == nullptr) return false;
        for (const auto &ent : list) {
            if (ent->id >= 0) continue;
            ent->id = ++maxId;
            ent->gid = gid;
            group->profiles.append(ent->id);
            ents[ent->id] = ent;
        }
        return true;
    }
    bool AddProfile(const std::shared_ptr<ProxyEntity> &ent, int gid = -1) {
        return AddProfileBatch(QList<std::shared_ptr<ProxyEntity>>{ent}, gid);
    }
    bool ReplaceProfile(int id, const std::shared_ptr<ProxyEntity> &ent) {
        auto old = GetProfile(id);
        if (id < 0 || ent == nullptr || ent->id >= 0 || old == nullptr) return false;
        ent->id = id;
        ent->gid = old->gid;
        ents[id] = ent;
        return true;
    }
    void lock() { maxLockDepth = std::max(maxLockDepth, ++lockDepth); }
    void unlock() { --lockDepth; }
} profileManagerValue;
ProfileManager *profileManager = &profileManagerValue;

struct BuildConfigResult { QString error; };
struct BuildConfigStatus { std::shared_ptr<BuildConfigResult> result = std::make_shared<BuildConfigResult>(); };

} // namespace Configs

namespace Subscription {
using namespace Configs;
int fixCalls = 0;
void RawUpdater_FixEnt(const std::shared_ptr<Configs::ProxyEntity> &) { ++fixCalls; }
RAW_UPDATER_DECL
} // namespace Subscription
