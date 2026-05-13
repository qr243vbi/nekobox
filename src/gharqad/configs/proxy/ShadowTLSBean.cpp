

#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/ProxyEntity.hpp>

#include <QStandardPaths>
#include <qjsonobject.h>

namespace Configs {

bool ShadowTLSBean::TryParseJson(const QJsonObject &obj) {
  using namespace Configs::From_Json;
  add_default_fields(this->entity, obj);
  password = obj["password"].toString();
  shadowtls_version = obj["version"].toInt();
  add_tls(stream, obj);
  return true;
}
bool ShadowTLSBean::TryParseLink(const QString &link) {
  using namespace From_Link;
  auto url = QUrl(link);
  if (!url.isValid())
    return false;
  auto query = GetQuery(url);
  add_default_fields(url, entity);

  password = url.userName();
  if (entity->serverPort == -1)
    entity->serverPort = 443;
  this->shadowtls_version = GetQueryIntValue(query, "version", 0);
  // security
  add_tls(stream, query);

  return !(password.isEmpty() || entity->serverAddress.isEmpty());
}

QString ShadowTLSBean::ToShareLink() const {
  using namespace Configs::To_Link;

  QUrl url;
  QUrlQuery query;
  url.setScheme("shadowtls");
  url.setUserName(password);
  add_default_fields(url, this);
  add_query_int("version", query, shadowtls_version);
  add_tls(stream, query);
  url.setQuery(query);
  return url.toString(QUrl::FullyEncoded);
}
CoreObjOutboundBuildResult ShadowTLSBean::BuildCoreObjSingBox() const {
  CoreObjOutboundBuildResult result;
  using namespace To_CoreObj_box;
  QJsonObject outbound{
      {"password", password},
      {"version", shadowtls_version},
  };

  add_default_fields(outbound, this);

  stream->BuildStreamSettingsSingBox(&outbound);
  result.outbound = outbound;
  return result;
}
} // namespace Configs