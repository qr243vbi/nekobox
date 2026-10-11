#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <iostream>
#include <memory>
// Test-only bean ownership and enum boundaries. No application or database.
struct TextEnum : QString {
    using QString::QString;
    using QString::operator=;
    int value = 0;
    explicit TextEnum(int) {}
};
using V2RAYTransportsEnum = TextEnum;
using VmessPacketEncodingsEnum = TextEnum;
struct KCPExtra {};
#define DECLARE_STORE_TYPE(...)
QString GetQueryValue(const QUrlQuery &, const QString &, const QString & = "");
namespace Configs {
struct V2rayStreamSettings { STREAM_FIELDS };
struct DataStore { QString utlsFingerprint = "firefox"; } defaults;
auto dataStore = &defaults;
struct ProxyEntity { QString name, serverAddress; int serverPort = -1; };
struct AbstractBean {
    ProxyEntity *entity;
    int mux_state = 0;
    explicit AbstractBean(ProxyEntity *e) : entity(e) {}
};
struct TrojanVLESSBean : AbstractBean {
    static constexpr int proxy_Trojan = 0, proxy_VLESS = 1;
    int proxy_type;
    QString password, flow, encryption;
    std::shared_ptr<TextEnum> network = std::make_shared<TextEnum>(0);
    std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();
    TrojanVLESSBean(ProxyEntity *e, int type) : AbstractBean(e), proxy_type(type) {}
    bool TryParseLink(const QString &);
    QString ToShareLink() const;
};
namespace From_Link {
void add_tls(std::shared_ptr<V2rayStreamSettings>, QUrlQuery &);
void add_default_fields(QUrl &, ProxyEntity *);
void add_mux_state(AbstractBean *, const QUrlQuery &);
}
namespace To_Link {
void add_query_nonempty(const char *, QUrlQuery &, const QString &);
}
}
