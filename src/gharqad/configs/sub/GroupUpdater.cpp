#ifdef _WIN32
#include <winsock2.h>
#endif


#include <nekobox/dataStore/Database.hpp>

#include <yaml-cpp/yaml.h>
#include <QMutex>
#include <QThreadPool>
#include <nekobox/configs/ConfigBuilder.hpp>
#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/configs/sub/GroupUpdater.hpp>
#include <nekobox/dataStore/ProfileFilter.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/global/HTTPRequestHelper.hpp>
#include <QUrl>

namespace Subscription {

GroupUpdater *groupUpdater = new GroupUpdater;

void RawUpdater_FixEnt(const std::shared_ptr<Configs::ProxyEntity> &ent) {
  auto bean = ent->unlock(ent->bean());
  if (ent == nullptr)
    return;
  auto stream = Configs::GetStreamSettings(bean.get());
  if (stream == nullptr)
    return;
  // 1. "security"
  if (stream->security == "none" || stream->security == "0" ||
      stream->security == "false") {
    stream->security = "";
  } else if (stream->security == "1" || stream->security == "true") {
    stream->security = "tls";
  }
  // 2. TLS SNI: v2rayN config builder generate sni like this, so set sni here
  // for their format.
  if (stream->security == "tls" && IsIpAddress(ent->serverAddress) &&
      (!stream->host.isEmpty()) && stream->sni.isEmpty()) {
    stream->sni = stream->host;
  }
}

int JsonEndIdx(const QString &str, int begin) {
  int sz = str.length();
  int counter = 1;
  for (int i = begin + 1; i < sz; i++) {
    if (str[i] == '{')
      counter++;
    if (str[i] == '}')
      counter--;
    if (counter == 0)
      return i;
  }
  return -1;
}

QList<QString> Disect(const QString &str) {
  QList<QString> res = QList<QString>();
  int idx = 0;
  int sz = str.size();
  while (idx < sz) {
    if (str[idx] == '\n') {
      idx++;
      continue;
    }
    if (str[idx] == '{') {
      int endIdx = JsonEndIdx(str, idx);
      if (endIdx == -1)
        return res;
      res.append(str.mid(idx, endIdx - idx + 1));
      idx = endIdx + 1;
      continue;
    }
    int nlineIdx = str.indexOf('\n', idx);
    if (nlineIdx == -1)
      nlineIdx = sz;
    res.append(str.mid(idx, nlineIdx - idx));
    idx = nlineIdx + 1;
  }
  return res;
}

void RawUpdater::update(const QString &str3) {
  // Base64 encoded subscription
  QList<QString> stack = {str3};
  QString str;

  std::shared_ptr<Configs::ProxyEntity> ent;
  bool needFix = true;

  bool json_contains_outbounds;
  bool json_contains_inbounds;
  bool json_contains_endpoints;
  QJsonParseError error{};
  QJsonDocument jsonDocument;
  bool json_ok;
  QJsonObject json;

ret_loop:
  if (stack.size() != 0) {
    str = stack.takeFirst();
  } else {
    return;
  }

  {
    int i = 32;
  loop:
    if (i > 0) {
      if (auto str2 = DecodeB64IfValid(str); !str2.isEmpty()) {
        str = str2;
        i--;
        goto loop;
      }
    }
  }

  jsonDocument = QJsonDocument::fromJson(str.toUtf8(), &error);
  json_ok = error.error == error.NoError;
  if (json_ok) {
    if (jsonDocument.isArray()) {
      for (auto i : jsonDocument.array()) {
        if (i.isObject()) {
          auto json = i.toObject();
          if (json.contains("proxy")) {
            stack << json["proxy"].toString();
          }
        } else if (i.isString()) {
          stack << i.toString();
        }
      }
      goto ret_loop;
    } else if (!jsonDocument.isObject()) {
      goto ret_loop;
    } else {
      auto json = jsonDocument.object();
    }
  }
  if (json_ok) {
    json_contains_endpoints = json.contains("endpoints");
    json_contains_outbounds = json.contains("outbounds");
    json_contains_inbounds = json.contains("inbounds");
    // SingBox
    if ((json_contains_outbounds || json_contains_endpoints)) {
      updateSingBox(json);
      goto ret_loop;
    }

    // SIP008
    if (json.contains("version") && json.contains("servers")) {
      updateSIP008(json);
      goto ret_loop;
    }
    goto parse_json;
  }

  // Clash
  if (updateClash(str)) {
    goto ret_loop;
  }

  // Wireguard Config
  /*
  if (updateWireguardFileConfig(str)){
    goto ret_loop;
  }
  */

  // Multi line
  if (str.count("\n") > 0) {
    auto list = Disect(str);
    for (const auto &str2 : list) {
      stack << str2.trimmed();
    }
    goto ret_loop;
  }

parse_json:

  // Json
  if (json_ok) {
    ent = Configs::ProfileManager::NewProxyEntity("custom");
    auto bean = ent->unlock(ent->CustomBean());
    if ((json_contains_outbounds || json_contains_endpoints) &&
        (json_contains_inbounds || json_contains_endpoints)) {
      bean->core = "internal-full";
      bean->config_simple = str;
    } else if (json.contains("type")) {
      bean->core = "internal";
      bean->config_simple = str;
    } else {
      goto ret_loop;
    }
  }
  
  #ifdef DEBUG_MODE
    qDebug() << "checker";
  #endif
  if (str.startsWith("//") || str.startsWith("#") || str.length() < 2) {
    goto ret_loop;
  }

  QString scheme;
  bool quic_enabled = false;
  int scheme_index = str.indexOf("://");
  if (scheme_index > 0) {
    scheme = str.sliced(0, scheme_index).toLower();
  } else {
    goto ret_loop;
  }

  // Nekoray format
  if (scheme == "nekoray") {
    needFix = false;
    auto link = QUrl(str);
    if (!link.isValid()) {
      goto ret_loop;
    }
    ent = Configs::ProfileManager::NewProxyEntity(link.host());
    if (!ent->isValid()) {
      goto ret_loop;
    }
    if (!ent->unlock(ent->bean())->TryParseNekorayLink(link)) {
      goto ret_loop;
    };
  }

  // SOCKS
  else if (scheme == ("socks5") || scheme == ("socks4") ||
           scheme == ("socks4a") || scheme == ("socks")) {
    ent = Configs::ProfileManager::NewProxyEntity("socks");
    auto ok = ent->unlock(ent->SocksBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // HTTP
  else if (scheme == ("http") || scheme == ("https")) {
    ent = Configs::ProfileManager::NewProxyEntity("http");
    auto ok = ent->unlock(ent->HttpBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // ShadowSocks
  else if (scheme == ("ss")) {
    ent = Configs::ProfileManager::NewProxyEntity("shadowsocks");
    auto ok = ent->unlock(ent->ShadowSocksBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // VMess
  else if (scheme == ("vmess")) {
    ent = Configs::ProfileManager::NewProxyEntity("vmess");
    auto ok = ent->unlock(ent->VMessBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // VLESS
  else if (scheme == ("vless")) {
    ent = Configs::ProfileManager::NewProxyEntity("vless");
    auto ok = ent->unlock(ent->TrojanVLESSBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // Trojan
  else if (scheme == ("trojan")) {
    ent = Configs::ProfileManager::NewProxyEntity("trojan");
    auto ok = ent->unlock(ent->TrojanVLESSBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // Mieru
  else if (scheme == ("mierus") || str.startsWith("mieru")) {
    ent = Configs::ProfileManager::NewProxyEntity("mieru");
    auto ok = ent->unlock(ent->MieruBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // Naive
  else if ((scheme == ("naive") || scheme == ("naive+https") ||
            scheme == ("naive+http")) ||
           (quic_enabled = scheme == ("naive+quic"))) {
    ent = Configs::ProfileManager::NewProxyEntity("naive");
    auto bean = ent->unlock(ent->NaiveBean());
    if (quic_enabled) {
      bean->quic = true;
      *bean->quic_congestion_control = 1;
    }
    auto ok = bean->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // TrustTunnel
  else if (scheme == ("tt")) {
    ent = Configs::ProfileManager::NewProxyEntity("trusttunnel");
    auto ok = ent->unlock(ent->TrustTunnelBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // Juicity
  else if (scheme == ("juicity")) {
    ent = Configs::ProfileManager::NewProxyEntity("juicity");
    auto ok = ent->unlock(ent->JuicityBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // AnyTLS
  else if (scheme == ("anytls")) {
    ent = Configs::ProfileManager::NewProxyEntity("anytls");
    auto ok = ent->unlock(ent->AnyTLSBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // ShadowTLS
  else if (scheme == ("shadowtls")) {
    ent = Configs::ProfileManager::NewProxyEntity("shadowtls");
    auto ok = ent->unlock(ent->ShadowTLSBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // Hysteria1
  else if (scheme == ("hysteria")) {
    needFix = false;
    ent = Configs::ProfileManager::NewProxyEntity("hysteria");
    auto ok = ent->unlock(ent->QUICBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // Hysteria2
  else if (scheme == ("hysteria2") || scheme == ("hy2")) {
    needFix = false;
    ent = Configs::ProfileManager::NewProxyEntity("hysteria2");
    auto ok = ent->unlock(ent->QUICBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // TUIC
  else if (scheme == ("tuic")) {
    needFix = false;
    ent = Configs::ProfileManager::NewProxyEntity("tuic");
    auto ok = ent->unlock(ent->QUICBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // Wireguard
  else if (scheme == ("wg")) {
    needFix = false;
    ent = Configs::ProfileManager::NewProxyEntity("wireguard");
    auto ok = ent->unlock(ent->WireguardBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // SSH
  else if (scheme == ("ssh")) {
    needFix = false;
    ent = Configs::ProfileManager::NewProxyEntity("ssh");
    auto ok = ent->unlock(ent->SSHBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  // tor
  else if (scheme == ("tor")) {
    needFix = false;
    ent = Configs::ProfileManager::NewProxyEntity("tor");
    auto ok = ent->unlock(ent->TorBean())->TryParseLink(str);
    if (!ok)
      goto ret_loop;
  }

  if (ent == nullptr)
    goto ret_loop;

  // Fix
  if (needFix)
    RawUpdater_FixEnt(ent);

  // End
  AddProxy(ent);

  goto ret_loop;
}

void RawUpdater::updateSIP008(const QJsonObject &json) {
  for (auto o : json["servers"].toArray()) {
    auto out = o.toObject();
    if (out.isEmpty()) {
      MW_show_log("invalid server object");
      continue;
    }

    auto ent = Configs::ProfileManager::NewProxyEntity("shadowsocks");
    auto ok = ent->unlock(ent->ShadowSocksBean())->TryParseFromSIP008(out);
    if (ok) {
      AddProxy(ent);
    }
  }
}

void RawUpdater::updateSingBox(const QJsonObject &json) {
  auto outbounds = json["outbounds"].toArray();
  auto endpoints = json["endpoints"].toArray();
  QJsonArray items;
  for (auto &&outbound : outbounds) {
    if (!outbound.isObject())
      continue;
    items.append(outbound.toObject());
  }
  for (auto &&endpoint : endpoints) {
    if (!endpoint.isObject())
      continue;
    items.append(endpoint.toObject());
  }

  for (auto o : items) {
    auto out = o.toObject();
    if (out.isEmpty()) {
      MW_show_log(QString("invalid outbound of type: ") +
                  QJsonType2QString(o.type()));
      continue;
    }

    std::shared_ptr<Configs::ProxyEntity> ent;
    QString out_type = out["type"].toString();

    // All Types
    if (Preset::SingBox::OutboundTypes.contains(out_type)) {
      ent = Configs::ProfileManager::NewProxyEntity(out_type);
      auto ok = ent->unlock(ent->bean())->TryParseJson(out);
      if (!ok)
        continue;
    }

    if (ent == nullptr)
      continue;

    AddProxy(ent);
  }
}

bool RawUpdater::updateWireguardFileConfig(const QString &str) {
  auto ent = Configs::ProfileManager::NewProxyEntity("wireguard");
  auto ok = ent->unlock(ent->WireguardBean())->TryParseLink(str);
  if (!ok)
    return false;
  AddProxy(ent);
  return true;
}

QString Node2QString(const YAML::Node &n, const QString &def = "") {
    try {
        if (!n || !n.IsDefined()) {
            return def;
        }
        return QString::fromStdString(n.as<std::string>());
    } catch (const YAML::Exception &ex) {
        qDebug() << ex.what();
        return def;
    }
}
QStringList Node2QStringList(const YAML::Node &n) {
    try {
        if (!n || !n.IsSequence()) {
            return {};
        }

        QStringList list;
        for (const auto &item : n) {
            list << QString::fromStdString(item.as<std::string>());
        }
        return list;

    } catch (const YAML::Exception &ex) {
        qDebug() << ex.what();
        return {};
    }
}


int Node2Int(const YAML::Node &n, int def = 0) {
    try {
        if (!n || !n.IsDefined()) {
            return def;
        }

        if (n.IsScalar()) {
            // yaml-cpp can convert directly if it's numeric
            try {
                return n.as<int>();
            } catch (const YAML::BadConversion &) {
                // fallback: parse string manually
                return std::atoi(n.as<std::string>().c_str());
            }
        }

        return def;

    } catch (const YAML::Exception &ex) {
        qDebug() << ex.what();
        return def;
    }
}
bool Node2Bool(const YAML::Node &n, bool def = false) {
    try {
        if (!n || !n.IsDefined()) {
            return def;
        }

        // First attempt: direct bool conversion
        try {
            return n.as<bool>();
        } catch (const YAML::BadConversion &) {
            // fall through to integer attempt
        }

        // Second attempt: interpret as integer
        try {
            return n.as<int>() != 0;
        } catch (const YAML::BadConversion &) {
            // fall through to default
        }

        return def;

    } catch (const YAML::Exception &ex) {
        qDebug() << ex.what();
        return def;
    }
}

YAML::Node NodeChild(const YAML::Node &n,
                     const std::list<std::string> &keys) {
    for (const auto &key : keys) {
        YAML::Node child = n[key];
        if (child && child.IsDefined()) {
            return child;
        }
    }
    return YAML::Node();  // null node
}

// https://github.com/Dreamacro/clash/wiki/configuration
bool RawUpdater::updateClash(const QString &str) {
  try {
    YAML::Node yaml_node = YAML::Load(str.toStdString());
    YAML::Node proxies   = yaml_node["proxies"];
    if (proxies.IsNull()){
      return false;
    }
    bool ret = false;
    for (auto proxy : proxies) {
      #ifdef DEBUG_MODE
        qDebug() << "Clash iterating";
      #endif
      ret = true;
      auto type = Node2QString(proxy["type"]).toLower();
      auto type_clash = type;

      if (type == "ss" || type == "ssr")
        type = "shadowsocks";
      if (type == "socks5")
        type = "socks";

      auto ent = Configs::ProfileManager::NewProxyEntity(type);
      if (!ent->isValid())
        continue;
      bool needFix = false;

      // common
      ent->name = Node2QString(proxy["name"]);
      ent->serverAddress = Node2QString(proxy["server"]);
      ent->serverPort = Node2Int(proxy["port"]);

      if (type_clash == "ss") {
        auto bean = ent->unlock(ent->ShadowSocksBean());
        bean->method = Node2QString(proxy["cipher"]).replace("dummy", "none");
        bean->password = Node2QString(proxy["password"]);

        // UDP over TCP
        if (Node2Bool(proxy["udp-over-tcp"])) {
          bean->uot = Node2Int(proxy["udp-over-tcp-version"]);
          if (bean->uot == 0)
            bean->uot = 2;
        }
        *bean->network = Node2QString(proxy["network"]);

        if (proxy["plugin"].IsDefined() && proxy["plugin-opts"].IsDefined()) {
          auto plugin_n = proxy["plugin"];
          auto pluginOpts_n = proxy["plugin-opts"];
          QStringList ssPlugin;
          auto plugin = Node2QString(plugin_n);
          if (plugin == "obfs") {
            ssPlugin << "obfs-local";
            ssPlugin << "obfs=" + Node2QString(pluginOpts_n["mode"]);
            ssPlugin << "obfs-host=" + Node2QString(pluginOpts_n["host"]);
          } else if (plugin == "v2ray-plugin") {
            auto mode = Node2QString(pluginOpts_n["mode"]);
            auto host = Node2QString(pluginOpts_n["host"]);
            auto path = Node2QString(pluginOpts_n["path"]);
            ssPlugin << "v2ray-plugin";
            if (!mode.isEmpty() && mode != "websocket")
              ssPlugin << "mode=" + mode;
            if (Node2Bool(pluginOpts_n["tls"]))
              ssPlugin << "tls";
            if (!host.isEmpty())
              ssPlugin << "host=" + host;
            if (!path.isEmpty())
              ssPlugin << "path=" + path;
            // clash only: skip-cert-verify
            // clash only: headers
            // clash: mux=?
          }
          bean->plugin = ssPlugin.join(";");
        }

        // sing-mux
        auto smux = NodeChild(proxy, {"smux"});
        if (!smux.IsNull() && Node2Bool(smux["enabled"]))
          bean->mux_state = 1;
        bean.reset();
      } else if (type == "socks") {
        auto bean = ent->unlock(ent->SocksBean());
        bean->username = Node2QString(proxy["username"]);
        bean->password = Node2QString(proxy["password"]);
        bean.reset();
      } else if (type == "http") {
        auto bean = ent->unlock(ent->HttpBean());
        bean->username = Node2QString(proxy["username"]);
        bean->password = Node2QString(proxy["password"]);
        if (type == "http" && Node2Bool(proxy["tls"])) {
          bean->stream->security = "tls";
          if (Node2Bool(proxy["skip-cert-verify"]))
            bean->stream->allow_insecure = true;
          bean->stream->sni = FIRST_OR_SECOND(
              Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
          bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
          bean->stream->utlsFingerprint =
              Node2QString(proxy["client-fingerprint"]);
          if (bean->stream->utlsFingerprint.isEmpty()) {
            bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
          }

          auto reality = NodeChild(proxy, {"reality-opts"});
          if (reality.IsMap()) {
            bean->stream->reality_pbk = Node2QString(reality["public-key"]);
            bean->stream->reality_sid = Node2QString(reality["short-id"]);
          }
        }
        bean.reset();
      } else if (type == "trojan" || type == "vless") {
        needFix = true;
        auto bean = ent->unlock(ent->TrojanVLESSBean());
        if (type == "vless") {
          bean->flow = Node2QString(proxy["flow"]);
          bean->password = Node2QString(proxy["uuid"]);
          // meta packet encoding
          if (Node2Bool(proxy["packet-addr"])) {
            *bean->stream->packet_encoding = "packetaddr";
          } else {
            // For VLESS, default to use xudp
            *bean->stream->packet_encoding = "xudp";
          }
        } else {
          bean->password = Node2QString(proxy["password"]);
        }
        bean->stream->security = "tls";
        *bean->stream->network = Node2QString(proxy["network"], "tcp");
        bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]),
                                            Node2QString(proxy["servername"]));
        bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
        bean->stream->allow_insecure = Node2Bool(proxy["skip-cert-verify"]);
        bean->stream->utlsFingerprint =
            Node2QString(proxy["client-fingerprint"]);
        if (bean->stream->utlsFingerprint.isEmpty()) {
          bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
        }

        // sing-mux
        auto smux = NodeChild(proxy, {"smux"});
        if (!smux.IsNull() && Node2Bool(smux["enabled"]))
          bean->mux_state = 1;

        // opts
        auto ws = NodeChild(proxy, {"ws-opts", "ws-opt"});
        if (ws.IsMap()) {
          auto headers = ws["headers"];
          if (headers.IsMap()) {
            for (auto header : headers) {
              if (Node2QString(header.first).toLower() == "host") {
                if (header.second.IsScalar())
                  bean->stream->host = Node2QString(header.second);
                else if (header.second.IsSequence() &&
                         header.second[0].IsScalar())
                  bean->stream->host = Node2QString(header.second[0]);
                break;
              }
            }
          }
          bean->stream->path = Node2QString(ws["path"]);
          bean->stream->ws_early_data_length = Node2Int(ws["max-early-data"]);
          bean->stream->ws_early_data_name =
              Node2QString(ws["early-data-header-name"]);
        }

        auto grpc = NodeChild(proxy, {"grpc-opts", "grpc-opt"});
        if (grpc.IsMap()) {
          bean->stream->path = Node2QString(grpc["grpc-service-name"]);
        }

        auto reality = NodeChild(proxy, {"reality-opts"});
        if (reality.IsMap()) {
          bean->stream->reality_pbk = Node2QString(reality["public-key"]);
          bean->stream->reality_sid = Node2QString(reality["short-id"]);
        }
        bean.reset();
      } else if (type == "vmess") {
        needFix = true;
        auto bean = ent->unlock(ent->VMessBean());
        bean->uuid = Node2QString(proxy["uuid"]);
        bean->aid = Node2Int(proxy["alterId"]);
        bean->security = Node2QString(proxy["cipher"], bean->security);
        *bean->stream->network =
            Node2QString(proxy["network"], "tcp").replace("h2", "http");
        bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]),
                                            Node2QString(proxy["servername"]));
        bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
        if (Node2Bool(proxy["tls"]))
          bean->stream->security = "tls";
        if (Node2Bool(proxy["skip-cert-verify"]))
          bean->stream->allow_insecure = true;
        bean->stream->utlsFingerprint =
            Node2QString(proxy["client-fingerprint"]);
        if (bean->stream->utlsFingerprint.isEmpty()) {
          bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
        }

        // sing-mux
        auto smux = NodeChild(proxy, {"smux"});
        if (!smux.IsNull() && Node2Bool(smux["enabled"]))
          bean->mux_state = 1;

        // meta packet encoding
        if (Node2Bool(proxy["xudp"]))
          *bean->stream->packet_encoding = "xudp";
        if (Node2Bool(proxy["packet-addr"]))
          *bean->stream->packet_encoding = "packetaddr";

        // opts
        auto ws = NodeChild(proxy, {"ws-opts", "ws-opt"});
        if (ws.IsMap()) {
          auto headers = ws["headers"];
          if (headers.IsMap()) {
            for (auto header : headers) {
              if (Node2QString(header.first).toLower() == "host") {
                bean->stream->host = Node2QString(header.second);
                break;
              }
            }
          }
          bean->stream->path = Node2QString(ws["path"]);
          bean->stream->ws_early_data_length = Node2Int(ws["max-early-data"]);
          bean->stream->ws_early_data_name =
              Node2QString(ws["early-data-header-name"]);
          // for Xray
          if (Node2QString(ws["early-data-header-name"]) ==
              "Sec-WebSocket-Protocol") {
            bean->stream->path += "?ed=" + Node2QString(ws["max-early-data"]);
          }
        }

        auto grpc = NodeChild(proxy, {"grpc-opts", "grpc-opt"});
        if (grpc.IsMap()) {
          bean->stream->path = Node2QString(grpc["grpc-service-name"]);
        }

        auto h2 = NodeChild(proxy, {"h2-opts", "h2-opt"});
        if (h2.IsMap()) {
          auto hosts = h2["host"];
          for (auto host : hosts) {
            bean->stream->host = Node2QString(host);
            break;
          }
          bean->stream->path = Node2QString(h2["path"]);
        }
        auto tcp_http = NodeChild(proxy, {"http-opts", "http-opt"});
        if (tcp_http.IsMap()) {
          *bean->stream->network = "tcp";
          bean->stream->header_type = "http";
          auto headers = tcp_http["headers"];
          if (headers.IsMap()) {
            for (auto header : headers) {
              if (Node2QString(header.first).toLower() == "host") {
                bean->stream->host = Node2QString(header.second);
                break;
              }
            }
          }
          auto paths = tcp_http["path"];
          if (paths.IsScalar())
            bean->stream->path = Node2QString(paths);
          else if (paths.IsSequence() && paths[0].IsScalar())
            bean->stream->path = Node2QString(paths[0]);
        }
        bean.reset();
      } else if (type == "anytls" || type == "shadowtls") {
        std::shared_ptr<Configs::AbstractBean> bean_common;
        needFix = true;
        std::shared_ptr<Configs::V2rayStreamSettings> stream;
        if (type == "anytls") {
          auto bean = ent->unlock(ent->AnyTLSBean());
          bean->password = Node2QString(proxy["password"]);
          stream = bean->stream;
          bean_common = bean;
        } else {
          auto bean = ent->unlock(ent->ShadowTLSBean());
          bean->password = Node2QString(proxy["password"]);
          bean->shadowtls_version = Node2Int(proxy["version"]);
          stream = bean->stream;
          bean_common = bean;
        }
        stream->security = "tls";
        if (Node2Bool(proxy["skip-cert-verify"]))
          stream->allow_insecure = true;
        stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]),
                                      Node2QString(proxy["servername"]));
        stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
        stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
        if (stream->utlsFingerprint.isEmpty()) {
          stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
        }

        auto reality = NodeChild(proxy, {"reality-opts"});
        if (reality.IsMap()) {
          stream->reality_pbk = Node2QString(reality["public-key"]);
          stream->reality_sid = Node2QString(reality["short-id"]);
        }
        bean_common.reset();
      } else if (type == "hysteria") {
        auto bean = ent->unlock(ent->QUICBean());

        bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
        auto alpn = Node2QStringList(proxy["alpn"]);
        bean->caText = Node2QString(proxy["ca-str"]);
        if (!alpn.isEmpty())
          bean->alpn = alpn[0];
        bean->sni = Node2QString(proxy["sni"]);

        auto auth_str = FIRST_OR_SECOND(Node2QString(proxy["auth_str"]),
                                        Node2QString(proxy["auth-str"]));
        auto auth = Node2QString(proxy["auth"]);
        if (!auth_str.isEmpty()) {
          bean->authPayloadType = Configs::QUICBean::hysteria_auth_string;
          bean->authPayload = auth_str;
        }
        if (!auth.isEmpty()) {
          bean->authPayloadType = Configs::QUICBean::hysteria_auth_base64;
          bean->authPayload = auth;
        }
        bean->obfsPassword = Node2QString(proxy["obfs"]);

        if (Node2Bool(proxy["disable_mtu_discovery"]) ||
            Node2Bool(proxy["disable-mtu-discovery"]))
          bean->disableMtuDiscovery = true;
        bean->streamReceiveWindow = Node2Int(proxy["recv-window"]);
        bean->connectionReceiveWindow = Node2Int(proxy["recv-window-conn"]);

        auto upMbps = Node2QString(proxy["up"]).split(" ")[0].toInt();
        auto downMbps = Node2QString(proxy["down"]).split(" ")[0].toInt();
        if (upMbps > 0)
          bean->uploadMbps = upMbps;
        if (downMbps > 0)
          bean->downloadMbps = downMbps;

        auto ports = Node2QString(proxy["ports"]);
        if (!ports.isEmpty()) {
          QStringList serverPorts;
          ports.replace("/", ",");
          for (const QString &port : ports.split(",", Qt::SkipEmptyParts)) {
            if (port.isEmpty()) {
              continue;
            }
            QString modifiedPort = port;
            modifiedPort.replace("-", ":");
            serverPorts.append(modifiedPort);
          }
          bean->serverPorts = serverPorts;
        }
        bean.reset();
      } else if (type == "hysteria2") {
        auto bean = ent->unlock(ent->QUICBean());

        bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
        bean->caText = Node2QString(proxy["ca-str"]);
        bean->sni = Node2QString(proxy["sni"]);

        bean->obfsPassword = Node2QString(proxy["obfs-password"]);
        bean->password = Node2QString(proxy["password"]);

        bean->uploadMbps = Node2QString(proxy["up"]).split(" ")[0].toInt();
        bean->downloadMbps = Node2QString(proxy["down"]).split(" ")[0].toInt();

        auto ports = Node2QString(proxy["ports"]);
        if (!ports.isEmpty()) {
          QStringList serverPorts;
          ports.replace("/", ",");
          for (const QString &port : ports.split(",", Qt::SkipEmptyParts)) {
            if (port.isEmpty()) {
              continue;
            }
            QString modifiedPort = port;
            modifiedPort.replace("-", ":");
            serverPorts.append(modifiedPort);
          }
          bean->serverPorts = serverPorts;
        }
        bean.reset();
      } else if (type == "tuic") {
        auto bean = ent->unlock(ent->QUICBean());

        bean->uuid = Node2QString(proxy["uuid"]);
        bean->password = Node2QString(proxy["password"]);

        if (Node2Int(proxy["heartbeat-interval"]) != 0) {
          bean->heartbeat =
              QString::number(Node2Int(proxy["heartbeat-interval"])) + "ms";
        }

        bean->udpRelayMode =
            Node2QString(proxy["udp-relay-mode"], bean->udpRelayMode);
        bean->congestionControl = Node2QString(proxy["congestion-controller"],
                                               bean->congestionControl);

        bean->disableSni = Node2Bool(proxy["disable-sni"]);
        bean->zeroRttHandshake = Node2Bool(proxy["reduce-rtt"]);
        bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
        bean->alpn = Node2QStringList(proxy["alpn"]).join(",");
        bean->caText = Node2QString(proxy["ca-str"]);
        bean->sni = Node2QString(proxy["sni"]);

        if (Node2Bool(proxy["udp-over-stream"]))
          bean->uos = true;

        if (!Node2QString(proxy["ip"]).isEmpty()) {
          if (bean->sni.isEmpty())
            bean->sni = bean->entity->serverAddress;
          bean->entity->serverAddress = Node2QString(proxy["ip"]);
        }
        bean.reset();
      } else {
        continue;
      }

      if (needFix)
        RawUpdater_FixEnt(ent);

      AddProxy(ent);
    }
    return ret;
  } catch (const YAML::Exception &ex) {
    //         runOnUiThread([=,this] {
  //  qDebug() << ("YAML Exception") << ex.what();
    //       });
    return false;
  }
  return true;
}

bool RawUpdater::AddProxy(std::shared_ptr<Configs::ProxyEntity> ent){
  auto key = Configs::ProfileFilterKey(ent, false);
  if (ignore_map.contains(key)){
    ignore_map[key] = true;
    return false;
  } else {
    proxies << ent;
    return true;
  }
};

//
void GroupUpdater::AsyncUpdate(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    const QString &str,
    const std::function<QString(bool *, bool *, const QString &)> &info,
    int _sub_gid, const std::function<void()> &finish) {
  auto content = str.trimmed();
  bool asURL = false;
  bool createNewGroup = false;
  QString groupName = "";

  if (_sub_gid < 0 &&
      (content.startsWith("http://") || content.startsWith("https://"))) {
    bool ok = false;
    groupName = info(&ok, &createNewGroup, content);
    asURL = true;
    if (ok == false) {
      return;
    }
    if (!createNewGroup && groupName == "link") {
      asURL = false;
    }
  }

  runOnNewThread([=, this] {
    auto gid = _sub_gid;
    if (createNewGroup) {
      auto group = Configs::ProfileManager::NewGroup();
      group->name = groupName;
      group->url = str;
      Configs::profileManager->AddGroup(group);
      gid = group->id;
      MW_dialog_message("SubUpdater", "NewGroup");
    }
    Update(PreFinishJob, str, gid, asURL, createNewGroup && groupName.isEmpty());
    emit asyncUpdateCallback(gid);
    if (finish != nullptr)
      finish();
  });
}

/*

*/

void GroupUpdater::Update(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    const QString &_str, int _sub_gid, bool _not_sub_as_url, bool _auto_name) {
  //
  Configs::dataStore->imported_count = 0;
  auto rawUpdater = std::make_unique<RawUpdater>();
  rawUpdater->gid_add_to = _sub_gid;

  //
  QString sub_user_info;
  bool asURL = _sub_gid >= 0 || _not_sub_as_url;
  auto content = _str.trimmed();
  auto originalUrl = content;
  auto group = Configs::profileManager->GetGroup(_sub_gid);
  if (group != nullptr && group->archive)
    return;

  //
  if (asURL) {
    auto groupName = group == nullptr ? content : group->name;
    MW_show_log(">>>>>>>> " +
                QObject::tr("Requesting subscription: %1").arg(groupName));

    auto resp = NetworkRequestHelper::HttpGet(
        content, Configs::dataStore->sub_send_hwid);
    if (!resp.error.isEmpty()) {
      MW_show_log("<<<<<<<< " +
                  QObject::tr("Requesting subscription %1 error: %2")
                      .arg(groupName, resp.error + "\n" + resp.data));
      return;
    }

    content = resp.data;
    sub_user_info =
        NetworkRequestHelper::GetHeader(resp.header, "Subscription-UserInfo");

    if (group != nullptr) {
      if ( group->name.isEmpty() && _auto_name){
        auto profileTitleRaw = NetworkRequestHelper::GetHeader(resp.header, "Profile-Title");
        if (!profileTitleRaw.isEmpty()) {
          QString profileTitle = profileTitleRaw.trimmed();
          int counter = 0;
          while (profileTitle.startsWith("base64:") && counter < 33) {
            counter ++;
            auto b64 = profileTitle.mid(7).toUtf8();
            auto decoded = QByteArray::fromBase64(b64, QByteArray::OmitTrailingEquals);
            if (!decoded.isEmpty()) {
              profileTitle = QString::fromUtf8(decoded).trimmed();
            }
          }
          group->name = profileTitle;
          MW_show_log("<<<<<<<< " + QObject::tr("Subscription profile title: %1").arg(profileTitle));
        } else {
          group->name = QUrl(originalUrl).host();
        }
      }
    }

    MW_show_log(
        "<<<<<<<< " +
        QObject::tr("Subscription request fininshed: %1").arg(groupName));
  }

  //
  //  QMap<Configs::ProfileFilterKey, bool> profile_map;

  /*
- Fix core dump (nullpointer exceptions)
- Improve table responsibility
- Improve ram usage
- Use LRU (Least Recently Used) cache for storing proxies
  */
  bool sub_clear = Configs::dataStore->sub_clear;
  rawUpdater->ignore_map.clear();
  QString change_text;
  QList<int> ProfilesToDrop;

  if (group != nullptr) {
    if (!sub_clear){
      if (group->profiles.count() > 1000){
        sub_clear = true;
      }
    }
    //
    if (sub_clear){
      MW_show_log(QObject::tr("Clearing servers..."));
      Configs::profileManager->BatchDeleteProfiles(group->profiles, -999);
      group->profiles.clear();
    } else {
      for (auto id : group->profiles){
        auto key = Configs::ProfileFilterKey(Configs::profileManager->GetProfile(id), false);
        // found duplicate profile
        if (rawUpdater->ignore_map.contains(key)){
          ProfilesToDrop << key.key->Id();
          change_text += "[-] " + key.key->DisplayTypeAndName() + "\n";
        }
        rawUpdater->ignore_map[ 
           key
        ] = false;
      }
    }

    group->sub_last_update = QDateTime::currentMSecsSinceEpoch() / 1000;
    group->info = sub_user_info;
    group->Save();
  }

  {
  auto count = rawUpdater->ignore_map.count() ;
  auto small_count = count < 150;
  MW_show_log(">>>>>>>> " + QObject::tr("Processing subscription data..."));
  rawUpdater->update(content);
  Configs::profileManager->AddProfileBatch(rawUpdater->proxies,
                                           rawUpdater->gid_add_to);

  if (count > 0){
    for ( auto [key, save] : asKeyValueRange(rawUpdater->ignore_map) ){
      if (!save){
        ProfilesToDrop << key.key->Id();
        if (small_count){
          change_text += "[-] " + key.key->DisplayTypeAndName() + "\n";
        }
      }
    }
    change_text += QObject::tr("--- Dropt %1 proxies\n").arg(ProfilesToDrop.count());
  }
  MW_show_log(">>>>>>>> " + QObject::tr("Process complete, applying..."));
  Configs::profileManager->BatchDeleteProfiles(ProfilesToDrop);
  }
  
  rawUpdater->ignore_map.clear();

  if (group != nullptr) {
    auto count = rawUpdater->proxies.count();
    // all is new profile
    if (count < 150){
      for (const auto &ent : rawUpdater->proxies) {
        change_text += "[+] " + ent->DisplayTypeAndName() + "\n";
      }
    } 
    change_text += QObject::tr("--- Added %1 proxies\n").arg(count);

    MW_show_log("<<<<<<<< " + QObject::tr("Change of %1:").arg(group->name) +
                "\n" + change_text);
    PreFinishJob(group);

    MW_dialog_message("SubUpdater", "finish-dingyue");
  } else {
    Configs::dataStore->imported_count = rawUpdater->proxies.count();
    MW_dialog_message("SubUpdater", "finish");
  }
}
} // namespace Subscription

bool UI_update_all_groups_Updating = false;

#define should_skip_group(g)                                                   \
  (g == nullptr || g->url.isEmpty() || g->archive ||                           \
   (onlyAllowed && g->skip_auto_update))

void serialUpdateSubscription(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    const QList<int> &groupsTabOrder,
    const std::function<QString(bool *, bool *, const QString &)> &info,
    int _order, bool onlyAllowed) {
  if (_order >= groupsTabOrder.size()) {
    UI_update_all_groups_Updating = false;
    return;
  }

  // calculate this group
  auto group = Configs::profileManager->GetGroup(groupsTabOrder[_order]);
  if (group == nullptr || should_skip_group(group)) {
    serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, _order + 1,
                             onlyAllowed);
    return;
  }

  int nextOrder = _order + 1;
  while (nextOrder < groupsTabOrder.size()) {
    auto nextGid = groupsTabOrder[nextOrder];
    auto nextGroup = Configs::profileManager->GetGroup(nextGid);
    if (!should_skip_group(nextGroup)) {
      break;
    }
    nextOrder += 1;
  }

  // Async update current group
  UI_update_all_groups_Updating = true;
  Subscription::groupUpdater->AsyncUpdate(
      PreFinishJob, group->url, info, group->id, [=] {
        serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, nextOrder,
                                 onlyAllowed);
      });
}

void UI_update_all_groups(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    bool onlyAllowed,
    const std::function<QString(bool *, bool *, const QString &)> &info) {
  if (UI_update_all_groups_Updating) {
    MW_show_log("The last subscription update has not exited.");
    return;
  }

  auto groupsTabOrder = Configs::profileManager->groupsTabOrder;
  serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, 0, onlyAllowed);
}
