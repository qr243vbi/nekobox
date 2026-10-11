#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QMap>
#include <QObject>
#include <iostream>
#include <memory>
#include <stdexcept>
// Enum storage, persistence, AddProxy filtering, postprocessing and unrelated
// import paths are test boundaries. Qt URL/query and the extracted dispatch are real.
struct TextEnum : QString { using QString::QString; using QString::operator=; explicit TextEnum(int) {} };
using NetworkEnum = TextEnum; using V2RAYTransportsEnum = TextEnum; using VmessPacketEncodingsEnum = TextEnum;
struct KCPExtra {};
#define DECLARE_STORE_TYPE(...)
QString GetQueryValue(const QUrlQuery &, const QString &, const QString & = "");
int GetQueryIntValue(const QUrlQuery &, const QString &, int = 0);
QByteArray DecodeB64IfValid(const QString &, QByteArray::Base64Options = QByteArray::Base64Encoding);
namespace Configs {
struct V2rayStreamSettings { STREAM_FIELDS };
struct DataStore { QString utlsFingerprint = "firefox"; } defaults;
auto dataStore = &defaults;
struct ProxyEntity;
struct AbstractBean {
    ProxyEntity *entity; int mux_state = 0;
    AbstractBean(ProxyEntity *e, int = 0) : entity(e) {}
    virtual ~AbstractBean() = default;
    virtual bool TryParseLink(const QString &) { throw std::runtime_error("unexpected parser"); }
    bool TryParseNekorayLink(const QUrl &) { throw std::runtime_error("unexpected native format"); }
    void Load() { throw std::runtime_error("persistence forbidden"); }
    void save_control_no_save(bool) {}
};
struct CustomBean : AbstractBean { using AbstractBean::AbstractBean; QString core, config_simple; };
struct ProxyEntity {
    std::weak_ptr<Configs::AbstractBean> weak_bean;
    std::shared_ptr<Configs::AbstractBean> strong_bean;
    QString type, display_type;
    ENTITY_FIELDS
    int id = -1;
    explicit ProxyEntity(const QString &type_);
    bool isValid() const;
    std::shared_ptr<const Configs::AbstractBean> bean() const;
    template<typename A> std::shared_ptr<A> unlock(std::shared_ptr<const A> bean);
    std::shared_ptr<const Configs::CustomBean> CustomBean() const { throw std::runtime_error("unexpected custom"); }
};
namespace Preset { namespace SingBox { QMap<QString, QString> OutboundTypes; } }
struct ProfileManager {
    static std::shared_ptr<ProxyEntity> NewProxyEntity(QString, bool = false);
    static QString GetDisplayType(const QString &);
};
namespace From_Link {
void set_boolean(const char *, bool &, const QUrlQuery &);
void add_tls(std::shared_ptr<V2rayStreamSettings>, QUrlQuery &);
void add_default_fields(QUrl &, ProxyEntity *);
void add_mux_state(AbstractBean *, const QUrlQuery &);
}
}
void MW_show_log(const QString &) { throw std::runtime_error("unexpected log path"); }
namespace HappDecrypt { QString decryptLink(const QString &) { throw std::runtime_error("unexpected encryption"); } }
QJsonObject sanitizeSingBoxConfig(const QJsonObject &) { throw std::runtime_error("unexpected JSON sanitizer"); }
namespace Subscription {
int fix_calls = 0;
void RawUpdater_FixEnt(const std::shared_ptr<Configs::ProxyEntity> &) { ++fix_calls; }
struct RawUpdater {
    QList<std::shared_ptr<Configs::ProxyEntity>> proxies;
    void update(const QString &);
    bool AddProxy(std::shared_ptr<Configs::ProxyEntity> p) { proxies.append(p); return true; }
    void updateSingBox(const QJsonObject &, const QString & = {}) { throw std::runtime_error("unexpected JSON import"); }
    void updateSIP008(const QJsonObject &) { throw std::runtime_error("unexpected SIP import"); }
    bool updateClash(const QString &) { throw std::runtime_error("unexpected YAML fallback"); }
    bool updateWireguardFileConfig(const QString &) { throw std::runtime_error("unexpected INI fallback"); }
};
}
