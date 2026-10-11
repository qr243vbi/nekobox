// Test-only profile/bean/format boundaries. String, JSON, and container behavior
// comes from real Qt6. No profile storage, application or network is used.
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QUrl>
#include <iostream>
#include <memory>
#include <utility>
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
