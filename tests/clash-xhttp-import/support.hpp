#include <QByteArray>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QMap>
#include <QString>
#include <QStringList>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <memory>
#include <list>
#include <cstdlib>
// Bean storage/reflection and application output are test-only boundaries.
// The complete updater and conversion helpers below are inserted verbatim.
using V2RAYTransportsEnum = QString;
struct VmessPacketEncodingsEnum : QString { using QString::operator=; explicit VmessPacketEncodingsEnum(int) {} };
#define DECLARE_STORE_TYPE(...)
struct KCPExtra { KCP_FIELDS };
#define FIRST_OR_SECOND(a,b) ((a).isEmpty() ? (b) : (a))
QString Node2QString(const YAML::Node &, const QString & = "");
QStringList Node2QStringList(const YAML::Node &);
int Node2Int(const YAML::Node &, int = 0);
bool Node2Bool(const YAML::Node &, bool = false);
YAML::Node NodeChild(const YAML::Node &, const std::list<std::string> &);
QByteArray DecodeB64IfValid(const QString &, QByteArray::Base64Options = QByteArray::Base64Encoding);
bool IsIpAddress(const QString &) { return false; } // Reserved-domain fixtures only.
QStringList messages;
void MW_show_log(const QString &s) { messages.append(s); }
namespace Configs {
struct V2rayStreamSettings { STREAM_FIELDS
 void BuildStreamSettingsSingBox(QJsonObject *);
 HEADER_PAIRS
};
struct DataStore { QString utlsFingerprint = "firefox"; bool skip_cert = false; } defaults;
auto dataStore = &defaults;
struct ProxyEntity;
struct AbstractBean {
    std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();
    std::shared_ptr<QString> network = std::make_shared<QString>();
    ProxyEntity *entity = nullptr;
    QString method, password, plugin, username, flow, uuid, security, alpn, sni, caText, authPayload, obfsPassword, heartbeat, udpRelayMode, congestionControl;
    QStringList serverPorts;
    bool allowInsecure = false, disableMtuDiscovery = false, disableSni = false, zeroRttHandshake = false, uos = false;
    int uot = 0, mux_state = 0, aid = 0, shadowtls_version = 0, authPayloadType = 0;
    int streamReceiveWindow = 0, connectionReceiveWindow = 0, uploadMbps = 0, downloadMbps = 0;
    static constexpr int hysteria_auth_string = 1, hysteria_auth_base64 = 2;
};
using QUICBean = AbstractBean;
struct ProxyEntity {
    QString name, serverAddress, type;
    int serverPort = -1;
    std::shared_ptr<AbstractBean> value = std::make_shared<AbstractBean>();
    explicit ProxyEntity(const QString &t) : type(t) {
        value->entity = this;
        if (!QStringList{"http", "trojan", "vless", "vmess", "anytls", "shadowtls"}.contains(t)) value->stream.reset();
    }
    bool isValid() const { return QStringList{"shadowsocks", "socks", "http", "trojan", "vless", "vmess", "anytls", "shadowtls", "hysteria", "hysteria2", "tuic"}.contains(type); }
    template<class T> std::shared_ptr<T> unlock(std::shared_ptr<T> b) { return b; }
    std::shared_ptr<AbstractBean> bean() { return value; }
    std::shared_ptr<AbstractBean> ShadowSocksBean() { return value; }
    std::shared_ptr<AbstractBean> SocksBean() { return value; }
    std::shared_ptr<AbstractBean> HttpBean() { return value; }
    std::shared_ptr<AbstractBean> TrojanVLESSBean() { return value; }
    std::shared_ptr<AbstractBean> VMessBean() { return value; }
    std::shared_ptr<AbstractBean> AnyTLSBean() { return value; }
    std::shared_ptr<AbstractBean> ShadowTLSBean() { return value; }
    std::shared_ptr<AbstractBean> QUICBean() { return value; }
};
struct ProfileManager {
    static std::shared_ptr<ProxyEntity> NewProxyEntity(const QString &type) { return std::make_shared<ProxyEntity>(type); }
};
V2rayStreamSettings *GetStreamSettings(AbstractBean *bean) { return bean ? bean->stream.get() : nullptr; }
}
namespace Subscription {
struct RawUpdater {
    QList<std::shared_ptr<Configs::ProxyEntity>> proxies;
    bool updateClash(const QString &);
    bool AddProxy(std::shared_ptr<Configs::ProxyEntity> entity) { proxies.append(entity); return true; }
};
}
