#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <iostream>
#include <memory>
#include <stdexcept>
// Test-only storage/enum boundaries. URL/query/JSON behavior is real Qt6.
struct TextEnum : QString {
    using QString::QString;
    using QString::operator=;
    int value = 0;
    explicit TextEnum(int v) : value(v) {}
};
using NetworkEnum = TextEnum;
using V2RAYTransportsEnum = TextEnum;
using VmessPacketEncodingsEnum = TextEnum;
struct KCPExtra {};
#define DECLARE_STORE_TYPE(...)
QString GetQueryValue(const QUrlQuery &, const QString &, const QString & = "");
int GetQueryIntValue(const QUrlQuery &, const QString &, int = 0);
namespace Configs {
struct V2rayStreamSettings {
STREAM_FIELDS
    void BuildStreamSettingsSingBox(QJsonObject *);
    QMap<QString, QString> GetHeaderPairs(bool *ok);
};
struct DataStore { QString utlsFingerprint = "firefox"; bool skip_cert = false; } defaults;
auto dataStore = &defaults;
struct ProxyEntity { QString name, serverAddress; int serverPort = -1; };
struct CoreObjOutboundBuildResult { QJsonObject outbound; };
struct AbstractBean {
    ProxyEntity *entity;
    int mux_state = 0;
    explicit AbstractBean(ProxyEntity *e, int) : entity(e) {}
    virtual QString type() const = 0;
    virtual ~AbstractBean() = default;
};
namespace From_Link {
void add_tls(std::shared_ptr<V2rayStreamSettings>, QUrlQuery &);
void add_default_fields(QUrl &, ProxyEntity *);
void add_mux_state(AbstractBean *, const QUrlQuery &);
}
namespace To_Link {
void add_query_nonempty(const char *, QUrlQuery &, const QString &);
}
namespace To_CoreObj_box {
// These unrelated transport branches are not fixture targets. Fail closed if
// a fixture unexpectedly reaches them; they never simulate transport behavior.
void parseExtraXhttp(QJsonObject &, QString) { throw std::runtime_error("unexpected xhttp branch"); }
void parseExtraKCP(QJsonObject &, std::shared_ptr<KCPExtra>) { throw std::runtime_error("unexpected kcp branch"); }
}
}
void MW_show_log(const char *) { throw std::runtime_error("unexpected header warning"); }
