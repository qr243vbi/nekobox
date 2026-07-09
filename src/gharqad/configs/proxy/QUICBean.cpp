#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/ProxyEntity.hpp>

#include <QStandardPaths>
#include <qjsonobject.h>

namespace Configs {

QString QUICBean::ToShareLink() const {
  using namespace Configs::To_Link;

  QUrl url;
  QString portRange;
  QUrlQuery query;
  if (proxy_type == proxy_Hysteria) {
    url.setScheme("hysteria");
    url.setHost(entity->serverAddress);
    url.setPort(0);
    add_query_int("upmbps", query, uploadMbps);
    add_query_int("downmbps", query, downloadMbps);
    if (!obfsPassword.isEmpty()) {
      query.addQueryItem("obfs", "xplus");
      query.addQueryItem("obfsParam", QUrl::toPercentEncoding(obfsPassword));
    }
    if (authPayloadType == hysteria_auth_string)
      query.addQueryItem("auth", authPayload);
    if (hyProtocol == hysteria_protocol_facktcp)
      query.addQueryItem("protocol", "faketcp");
    if (hyProtocol == hysteria_protocol_wechat_video)
      query.addQueryItem("protocol", "wechat-video");
    if (allowInsecure)
      query.addQueryItem("insecure", "1");
    add_query_nonempty("peer", query, sni);
    add_query_nonempty("alpn", query, alpn);
    add_query_int_natural("recv_window", query, (connectionReceiveWindow));
    add_query_int_natural("recv_window_conn", query, (streamReceiveWindow));
    if (!serverPorts.empty()) {
      QStringList portList;
      for (const auto &range : serverPorts) {
        QString modifiedRange = range;
        modifiedRange.replace(":", "-");
        portList.append(modifiedRange);
      }
      portRange = portList.join(",");
    } else
      url.setPort(entity->serverPort);
    if (!hop_interval.isEmpty())
      query.addQueryItem("hop_interval", hop_interval);
    if (!entity->name.isEmpty())
      url.setFragment(entity->name);
  } else if (proxy_type == proxy_TUIC) {
    url.setScheme("tuic");
    url.setUserName(uuid);
    url.setPassword(password);
    add_default_fields(url, this);

    if (!congestionControl.isEmpty())
      query.addQueryItem("congestion_control", congestionControl);
    if (!alpn.isEmpty())
      query.addQueryItem("alpn", alpn);
    if (!sni.isEmpty())
      query.addQueryItem("sni", sni);
    if (!udpRelayMode.isEmpty())
      query.addQueryItem("udp_relay_mode", udpRelayMode);
    if (allowInsecure)
      query.addQueryItem("allow_insecure", "1");
    if (disableSni)
      query.addQueryItem("disable_sni", "1");
  } else if (proxy_type == proxy_Hysteria2) {
    url.setScheme("hy2");
    url.setHost(entity->serverAddress);
    url.setPort(0);
    if (password.contains(":")) {
      url.setUserName(SubStrBefore(password, ":"));
      url.setPassword(SubStrAfter(password, ":"));
    } else {
      url.setUserName(password);
    }
    if (!obfsPassword.isEmpty()) {
      query.addQueryItem("obfs", "salamander");
      query.addQueryItem("obfs-password",
                         QUrl::toPercentEncoding(obfsPassword));
    }
    if (allowInsecure)
      query.addQueryItem("insecure", "1");
    if (!sni.isEmpty())
      query.addQueryItem("sni", sni);
    if (!serverPorts.empty()) {
      QStringList portList;
      for (const auto &range : serverPorts) {
        QString modifiedRange = range;
        modifiedRange.replace(":", "-");
        portList.append(modifiedRange);
      }
      portRange = portList.join(",");
    } else
      url.setPort(entity->serverPort);
    if (!hop_interval.isEmpty())
      query.addQueryItem("hop_interval", hop_interval);
    if (!entity->name.isEmpty())
      url.setFragment(entity->name);
  }
  add_network(query, this);
  if (!query.isEmpty())
    url.setQuery(query);

  if (portRange.isEmpty())
    return url.toString(QUrl::FullyEncoded);
  else
    return url.toString(QUrl::FullyEncoded)
        .replace(":0?", ":" + portRange + "?");
}

CoreObjOutboundBuildResult QUICBean::BuildCoreObjSingBox() const {
  using namespace To_CoreObj_box;
  CoreObjOutboundBuildResult result;

  QJsonObject coreTlsObj{
      {"enabled", true},           {"disable_sni", disableSni},
      {"insecure", allowInsecure}, {"certificate", caText.trimmed()},
      {"server_name", sni},
  };
  if (!alpn.trimmed().isEmpty())
    coreTlsObj["alpn"] = QListStr2QJsonArray(alpn.split(","));
  if (proxy_type == proxy_Hysteria2)
    coreTlsObj["alpn"] = "h3";

  QJsonObject outbound{
      {"tls", coreTlsObj},
  };
  add_default_fields(outbound, this);
  add_network(outbound, this);

  if (proxy_type == proxy_Hysteria) {
    outbound["obfs"] = obfsPassword;
    outbound["disable_mtu_discovery"] = disableMtuDiscovery;
    outbound["recv_window"] = streamReceiveWindow;
    outbound["recv_window_conn"] = connectionReceiveWindow;
    outbound["up_mbps"] = uploadMbps;
    outbound["down_mbps"] = downloadMbps;
    if (!serverPorts.empty()) {
      outbound.remove("server_port");
      QStringList modifiedPorts;
      for (const QString &port : serverPorts) {
        if (port.contains(":")) {
          modifiedPorts.append(port);
        } else {
          modifiedPorts.append(port + ":" + port);
        }
      }
      outbound["server_ports"] = QListStr2QJsonArray(modifiedPorts);
      add_non_empty(outbound, "hop_interval", hop_interval);
    }

    if (authPayloadType == hysteria_auth_base64)
      outbound["auth"] = authPayload;
    if (authPayloadType == hysteria_auth_string)
      outbound["auth_str"] = authPayload;
  } else if (proxy_type == proxy_Hysteria2) {
    outbound["password"] = password;
    outbound["up_mbps"] = uploadMbps;
    outbound["down_mbps"] = downloadMbps;
    if (!serverPorts.empty()) {
      outbound.remove("server_port");
      QStringList modifiedPorts;
      for (const QString &port : serverPorts) {
        if (port.contains(":")) {
          modifiedPorts.append(port);
        } else {
          modifiedPorts.append(port + ":" + port);
        }
      }
      outbound["server_ports"] = QListStr2QJsonArray(modifiedPorts);
      add_non_empty(outbound, "hop_interval", hop_interval);
    }

    if (!obfsPassword.isEmpty()) {
      outbound["obfs"] = QJsonObject{
          {"type", "salamander"},
          {"password", obfsPassword},
      };
    }
  } else if (proxy_type == proxy_TUIC) {
    outbound["uuid"] = uuid;
    outbound["password"] = password;
    outbound["congestion_control"] = congestionControl;
    if (uos) {
      outbound["udp_over_stream"] = true;
    } else {
      outbound["udp_relay_mode"] = udpRelayMode;
    }
    outbound["zero_rtt_handshake"] = zeroRttHandshake;
    add_non_empty(outbound, "heartbeat", heartbeat.trimmed());
  }

  result.outbound = outbound;
  return result;
}

bool QUICBean::TryParseJson(const Configs::Data::Node &obj) {
  using namespace Configs::From_Json;
  auto type = obj["type"].toString();
  add_network(this, obj);
  if (type == "hysteria") {
    proxy_type = proxy_Hysteria;
    auto &server_ports = obj["server_ports"];
    serverPorts =
        server_ports.isArray() ? server_ports.toStringList() : QStringList();
    hop_interval = obj["hop_interval"].toString();
    uploadMbps = obj["up_mbps"].isDouble() ? obj["up_mbps"].toInt() : 0;
    downloadMbps = obj["down_mbps"].isDouble() ? obj["down_mbps"].toInt() : 0;
    obfsPassword = obj["obfs"].toString();
    authPayloadType =
        obj["auth"].isString() ? hysteria_auth_base64 : hysteria_auth_string;
    auto &obj_auth = obj["auth"];
    authPayload = obj_auth.toString();
    disableMtuDiscovery = obj["disable_mtu_discovery"].toBool();
    connectionReceiveWindow = obj["recv_window_conn"].toInt();
    streamReceiveWindow = obj["recv_window"].toInt();
  } else if (type == "hysteria2") {
    proxy_type = proxy_Hysteria2;
    auto &server_ports = obj["server_ports"];
    serverPorts =
        server_ports.isArray() ? server_ports.toStringList() : QStringList();
    hop_interval = obj["hop_interval"].toString();
    uploadMbps = obj["up_mbps"].isDouble() ? obj["up_mbps"].toInt() : 0;
    downloadMbps = obj["down_mbps"].isDouble() ? obj["down_mbps"].toInt() : 0;
    obfsPassword = obj["obfs"]["password"].toString();
    password = obj["password"].toString();
  } else if (type == "tuic") {
    proxy_type = proxy_TUIC;
    uuid = obj["uuid"].toString();
    congestionControl = obj["congestion_control"].toString();
    udpRelayMode = obj["udp_relay_mode"].toString();
    uos = obj["udp_over_stream"].toBool();
    zeroRttHandshake = obj["zero_rtt_handshake"].toBool();
    heartbeat = obj["heartbeat"].toString();
    password = obj["password"].toString();
  } else {
    return false;
  }
  add_default_fields(this->entity, obj);
  auto &obj_tls = obj["tls"];
  alpn = obj_tls["alpn"].isArray() ? obj_tls["alpn"].toStringList().join(",")
                                     : obj_tls["alpn"].toString();
  sni = obj_tls["server_name"].toString();
  disableSni = obj_tls["disable_sni"].toBool();
  allowInsecure = obj_tls["insecure"].toBool();
  return true;
}

bool QUICBean::TryParseYaml(const Configs::Data::Node &obj) {
  using namespace Configs::From_Yaml;
  EnumFieldName type = obj["type"].toString();
  if (type == "tuic") {
    proxy_type = proxy_TUIC;
    uuid = obj[{"uuid", "token"}].toString();
    password = obj["password"].toString();
    heartbeat = obj["heartbeat-interval"].toString() + "s";
    congestionControl = obj["congestion-controler"].toString();
  } else if (type == "hysteria") {
    proxy_type = proxy_Hysteria;
    auto &server_ports = obj["ports"];
    serverPorts =
        server_ports.isArray() ? server_ports.toStringList() : QStringList();
    hop_interval = obj["hop-interval"].toString();
    uploadMbps = obj["up"].toInt();
    downloadMbps = obj["down"].toInt();
    obfsPassword = obj["obfs"].toString();
    authPayloadType = hysteria_auth_string;
    authPayload = obj["auth-str"].toString();
    disableMtuDiscovery = obj["disable_mtu_discovery"].toBool();
    connectionReceiveWindow = obj["recv-window-conn"].toInt();
    streamReceiveWindow = obj["recv-window"].toInt();
  } else if (type == "hysteria2") {
    proxy_type = proxy_Hysteria2;
    auto &server_ports = obj["ports"];
    serverPorts =
        server_ports.isArray() ? server_ports.toStringList() : QStringList();
    hop_interval = obj["hop-interval"].toString();
    uploadMbps = obj["up"].toInt();
    downloadMbps = obj["down"].toInt();
    obfsPassword = obj["obfs-password"].toString();
    password = obj["password"].toString();
  }
  add_default_fields(this->entity, obj);
  disableSni = obj["disable-sni"].toBool();
  alpn = obj["alpn"].isArray() ? obj["alpn"].toStringList().join(",")
                                     : obj["alpn"].toString();
  sni = obj["sni"].toString();
  allowInsecure = obj["skip-cert-verify"].toBool();
  return false;
}

bool QUICBean::TryParseLink(const QString &link) {
  using namespace From_Link;
  auto url = QUrl(link);
  if (!url.isValid()) {
    if (!url.errorString().startsWith("Invalid port"))
      return false;
    entity->serverPort = 0;
    serverPorts =
        QString::fromStdString(
            URLParser::Parse((link.split("?")[0] + "/").toStdString()).port)
            .split(",");
    for (int i = 0; i < serverPorts.size(); i++) {
      serverPorts[i].replace("-", ":");
    }
  }
  auto query = QUrlQuery(url.query());

  add_network(this, query);
  if (url.scheme() == "hysteria") {
    // https://hysteria.network/docs/uri-scheme/
    if (!query.hasQueryItem("upmbps") || !query.hasQueryItem("downmbps"))
      return false;

    entity->name = url.fragment(QUrl::FullyDecoded);
    entity->serverAddress = url.host();
    if (entity->serverPort > 0)
      entity->serverPort = url.port();
    obfsPassword =
        QUrl::fromPercentEncoding(query.queryItemValue("obfsParam").toUtf8());
    allowInsecure =
        QStringList{"1", "true"}.contains(query.queryItemValue("insecure"));
    uploadMbps = GetQueryIntValue(query, "upmbps");
    downloadMbps = GetQueryIntValue(query, "downmbps");

    auto protocolStr =
        (query.hasQueryItem("protocol") ? query.queryItemValue("protocol")
                                        : "udp")
            .toLower();
    if (protocolStr == "faketcp") {
      hyProtocol = Configs::QUICBean::hysteria_protocol_facktcp;
    } else if (protocolStr.startsWith("wechat")) {
      hyProtocol = Configs::QUICBean::hysteria_protocol_wechat_video;
    }

    if (query.hasQueryItem("auth")) {
      authPayload = query.queryItemValue("auth");
      authPayloadType = Configs::QUICBean::hysteria_auth_string;
    }

    alpn = query.queryItemValue("alpn");
    sni = FIRST_OR_SECOND(query.queryItemValue("peer"),
                          query.queryItemValue("sni"));

    connectionReceiveWindow = GetQueryIntValue(query, "recv_window");
    streamReceiveWindow = GetQueryIntValue(query, "recv_window_conn");

    if (query.hasQueryItem("mport")) {
      serverPorts = query.queryItemValue("mport").split(",");
      for (int i = 0; i < serverPorts.size(); i++) {
        serverPorts[i].replace("-", ":");
      }
    }
    hop_interval = query.queryItemValue("hop_interval");
  } else if (url.scheme() == "tuic") {
    // by daeuniverse
    // https://github.com/daeuniverse/dae/discussions/182
    add_default_fields(url, entity);

    if (entity->serverPort == -1)
      entity->serverPort = 443;

    uuid = url.userName();
    password = url.password();

    congestionControl = query.queryItemValue("congestion_control");
    alpn = query.queryItemValue("alpn");
    sni = query.queryItemValue("sni");
    udpRelayMode = query.queryItemValue("udp_relay_mode");
    allowInsecure = query.queryItemValue("allow_insecure") == "1";
    disableSni = query.queryItemValue("disable_sni") == "1";
  } else if (QStringList{"hy2", "hysteria2"}.contains(url.scheme())) {
    entity->name = url.fragment(QUrl::FullyDecoded);
    entity->serverAddress = url.host();
    if (entity->serverPort > 0)
      entity->serverPort = url.port();
    obfsPassword = QUrl::fromPercentEncoding(
        query.queryItemValue("obfs-password").toUtf8());
    allowInsecure =
        QStringList{"1", "true"}.contains(query.queryItemValue("insecure"));

    if (url.password().isEmpty()) {
      password = url.userName();
    } else {
      password = url.userName() + ":" + url.password();
    }
    if (query.hasQueryItem("mport")) {
      serverPorts = query.queryItemValue("mport").split(",");
      for (int i = 0; i < serverPorts.size(); i++) {
        serverPorts[i].replace("-", ":");
      }
    }
    hop_interval = query.queryItemValue("hop_interval");

    sni = query.queryItemValue("sni");
  }

  return true;
}

QString QUICBean::type() const {
  if (proxy_type == proxy_TUIC) {
    return "tuic";
  } else if (proxy_type == proxy_Hysteria) {
    return "hysteria";
  } else {
    return "hysteria2";
  }
}

QUICBean::QUICBean(Configs::ProxyEntity *entity, int _proxy_type)
    : AbstractBean(entity, 0) {
  proxy_type = _proxy_type;
  if (proxy_type == proxy_Hysteria || proxy_type == proxy_Hysteria2) {
    if (proxy_type == proxy_Hysteria) { // hy1
    } else {                            // hy2
      uploadMbps = 0;
      downloadMbps = 0;
    }
  } else if (proxy_type == proxy_TUIC) {
  }
}
} // namespace Configs