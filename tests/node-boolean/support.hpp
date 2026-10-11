// Explicit test boundary: real Qt, actual declarations, and extracted bodies.
// Map construction/access avoids the independent at(EnumFieldName) recursion.
// Composite quoting and map serialization deliberately throw if reached.
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <variant>
#include <tuple>
#include <memory>
#include <string>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <cmath>

ENUM_DECLARATIONS
// The actual declaration refers to this unrelated map-view type; no instance is used.
template<class T> class KeyValueRange;
namespace Configs { namespace Data {
NODE_DECLARATIONS
Node::Node(const QJsonObject &obj) : Node(Tag::Map) {
    auto &map = std::get<QMap<EnumFieldName, Node>>(value);
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        map.insert(EnumFieldName(it.key()), Node(it.value()));
}
const Node &Node::operator[](const EnumFieldName &key) const {
    return at(QList<EnumFieldName>{key});
}
QString Node::toQuoted() const { throw std::logic_error("Composite quoting outside this regression"); }
QVariantMap Node::toVariantMap() const { throw std::logic_error("Map serialization outside this regression"); }
}}
QString DecodeB64IfValid(const QString &) { throw std::logic_error("YAML ECH decoding outside this regression"); }
namespace Configs {
struct AbstractBean { int mux_state = -1; };
struct V2rayStreamSettings {
    bool enable_ech = false, enable_tls_fragment = false, enable_tls_record_fragment = false, allow_insecure = false;
    QString ech_config, query_server_name, security, reality_pbk, reality_sid, utlsFingerprint, tls_fragment_fallback_delay, sni, alpn;
};
namespace From_Json {
void add_mux_state(AbstractBean *, const Data::Node &);
int parseUOT(const Data::Node &);
bool add_tls(std::shared_ptr<V2rayStreamSettings>, const Data::Node &);
}
namespace From_Yaml {
void add_mux_state(AbstractBean *, const Data::Node &);
int parseUOT(const Data::Node &);
bool add_tls(std::shared_ptr<V2rayStreamSettings>, const Data::Node &);
}
}
