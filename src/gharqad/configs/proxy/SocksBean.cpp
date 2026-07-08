



#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/ProxyEntity.hpp>

#include <QStandardPaths>
#include <qjsonobject.h>

namespace Configs {

bool SocksBean::TryParseJson(const Configs::Data::Node &obj) {
  using namespace Configs::From_Json;
  add_default_fields(this->entity, obj);
  this->socks_http_type = obj["version"].getNumber(type_Socks5);
  add_username_password(this, obj);
  add_udp_over_tcp(this, obj);
  add_network(this, obj);
  return true;
}

bool SocksBean::TryParseYaml(const Configs::Data::Node &obj) {
  using namespace Configs::From_Yaml;
  add_default_fields(this->entity, obj);
  this->socks_http_type = type_Socks5;
  add_username_password(this, obj);
 // add_udp_over_tcp(this, obj);
  add_network(this, obj);
  return true;
}

QString SocksBean::ToShareLink() const {
  using namespace Configs::To_Link;

  QUrl url;
  QUrlQuery query;
  add_default_fields(url, this);
  {
    url.setScheme(QString("socks%1").arg(socks_http_type));
  }
  add_username_password(url, this);
  add_network(query, this);
  add_udp_over_tcp(query, this);
  url.setQuery(query);
  return url.toString(QUrl::FullyEncoded);
}

CoreObjOutboundBuildResult SocksBean::BuildCoreObjSingBox() const {
  CoreObjOutboundBuildResult result;
  using namespace To_CoreObj_box;
  QJsonObject outbound;
  outbound["version"] = QString::number(socks_http_type);
  add_default_fields(outbound, this);
  add_username_password(outbound, this);
  add_udp_over_tcp(outbound, this);
  add_network(outbound, this);
  result.outbound = outbound;
  return result;
}
    bool SocksBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        if (link.startsWith("socks4")) socks_http_type = type_Socks4;
        add_default_fields(url, entity);
        if (entity->serverPort == -1) entity->serverPort = 1080;

        add_username_password(this, url);
        add_network(this, query);
        add_udp_over_tcp(this, query);
        return !entity->serverAddress.isEmpty();
    }

} // namespace Configs