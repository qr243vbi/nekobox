#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <iostream>
#include <memory>
#include <stdexcept>
// Storage and enum boundaries only. URL/query/JSON/Base64 operations are real Qt.
struct TextEnum : QString {
    using QString::QString;
    using QString::operator=;
    int value = 0;
    explicit TextEnum(int v) : value(v) {}
    TextEnum &operator=(int v) { value = v; return *this; }
};
using NetworkEnum = TextEnum;
using MultiplexingEnum = TextEnum;
using QUICEnum = TextEnum;
using V2RAYTransportsEnum = TextEnum;
using VmessPacketEncodingsEnum = TextEnum;
struct KCPExtra {};
#define DECLARE_STORE_TYPE(...)
QString GetQueryValue(const QUrlQuery &, const QString &, const QString & = "");
int GetQueryIntValue(const QUrlQuery &, const QString &, int = 0);
QByteArray DecodeB64IfValid(const QString &, QByteArray::Base64Options = QByteArray::Base64Encoding);
namespace Configs {
struct V2rayStreamSettings { STREAM_FIELDS };
struct DataStore { QString utlsFingerprint = "firefox"; } defaults;
auto dataStore = &defaults;
struct ProxyEntity { QString name, serverAddress, type; int serverPort = -1; };
struct AbstractBean {
    ProxyEntity *entity;
    int mux_state = 0;
    explicit AbstractBean(ProxyEntity *e, int = 0) : entity(e) {}
};
namespace From_Link {
void set_boolean(const char *, bool &, const QUrlQuery &);
void add_tls(std::shared_ptr<V2rayStreamSettings>, QUrlQuery &);
void add_default_fields(QUrl &, ProxyEntity *);
void add_mux_state(AbstractBean *, const QUrlQuery &);
}
}
