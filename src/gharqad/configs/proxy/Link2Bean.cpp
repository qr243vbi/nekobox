#ifdef _WIN32
#include <winsock2.h>
#endif



#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/ProxyEntity.hpp>

#include <QUrlQuery>

namespace Configs {

void From_Link::set_boolean(const char *name, bool &value,
                            const QUrlQuery &obj) {
  auto qval = obj.queryItemValue(name);
  if (value) {
    value = !(qval.localeAwareCompare("false"));
  } else {
    value = (qval.localeAwareCompare("true"));
  }
}

void From_Link::add_mux_state(AbstractBean *bean, const QUrlQuery &query) {
  auto mux_str = GetQueryValue(query, "mux", "");
  if (mux_str == "true") {
    bean->mux_state = 1;
  } else if (mux_str == "false") {
    bean->mux_state = 2;
  } else {
    bean->mux_state = 0;
  }
}

void From_Link::add_tls(std::shared_ptr<V2rayStreamSettings> stream,
                        QUrlQuery &query) {
  stream->security = "tls";
  auto sni1 = GetQueryValue(query, "sni");
  auto sni2 = GetQueryValue(query, "peer");
  if (!sni1.isEmpty())
    stream->sni = sni1;
  if (!sni2.isEmpty())
    stream->sni = sni2;
  stream->alpn = GetQueryValue(query, "alpn");
  stream->allow_insecure =
      !QStringList{"0", "false"}.contains(query.queryItemValue("insecure"));
  auto ech_config = GetQueryValue(query, "ech_config");
  if ((stream->enable_ech = (!ech_config.isEmpty()))) {
    stream->ech_config = ech_config;
    stream->query_server_name = GetQueryValue(query, "query_server_name");
  }

  stream->reality_pbk = GetQueryValue(query, "pbk", "");
  stream->reality_sid = GetQueryValue(query, "sid", "");
  stream->utlsFingerprint = GetQueryValue(query, "fp", "");
  if (query.queryItemValue("fragment") == "1")
    stream->enable_tls_fragment = true;
  stream->tls_fragment_fallback_delay =
      query.queryItemValue("fragment_fallback_delay");
  if (query.queryItemValue("record_fragment") == "1")
    stream->enable_tls_record_fragment = true;
  if (stream->utlsFingerprint.isEmpty()) {
    stream->utlsFingerprint = dataStore->utlsFingerprint;
  }
}

void From_Link::add_default_fields(QUrl &url, Configs::ProxyEntity *entity) {
  entity->name = url.fragment(QUrl::FullyDecoded);
  entity->serverAddress = url.host();
  entity->serverPort = url.port();
}
} // namespace Configs
