#include <nekobox/dataStore/Database.hpp>
#include <yaml-cpp/yaml.h>
#include <QMutex>
#include <QThreadPool>
#include <nekobox/configs/ConfigBuilder.hpp>
#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/configs/sub/GroupUpdater.hpp>
#include <nekobox/configs/sub/HappDecrypt.hpp>
#include <nekobox/dataStore/ProfileFilter.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/dataStore/HTTPRequestHelper.hpp>
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


static QString jsonFirstString(const QJsonObject &obj,
                               const QStringList &keys) {
  for (const auto &key : keys) {
    auto value = obj[key];
    if (value.isString() && !value.toString().isEmpty()) {
      return value.toString();
    }
    if (value.isDouble()) {
      return QString::number(value.toInt());
    }
  }
  return {};
}

static int jsonFirstInt(const QJsonObject &obj, const QStringList &keys,
                        int def = 0) {
  for (const auto &key : keys) {
    auto value = obj[key];
    if (value.isDouble()) {
      return value.toInt();
    }
    if (value.isString()) {
      bool ok = false;
      auto number = value.toString().split(' ', Qt::SkipEmptyParts)
                        .value(0)
                        .toInt(&ok);
      if (ok) {
        return number;
      }
    }
  }
  return def;
}

static QJsonObject migrateDnsServerToSingBox112(QJsonObject server,
                                                const QJsonObject &fakeip) {
  if (server.contains("address_resolver") && !server.contains("domain_resolver")) {
    server["domain_resolver"] = server["address_resolver"];
  }
  server.remove("address_resolver");

  if (server.contains("type")) {
    const auto type = server["type"].toString();
    if (server["detour"].toString() == "direct" && type == "udp") {
      server["type"] = "local";
      server.remove("server");
      server.remove("server_port");
      server.remove("domain_resolver");
      server.remove("detour");
    } else if (type == "fakeip") {
      server.remove("domain_resolver");
      server.remove("detour");
      const auto inet4 = fakeip["inet4_range"].toString();
      const auto inet6 = fakeip["inet6_range"].toString();
      if (!inet4.isEmpty() && server["inet4_range"].toString().isEmpty()) {
        server["inet4_range"] = inet4;
      }
      if (!inet6.isEmpty() && server["inet6_range"].toString().isEmpty()) {
        server["inet6_range"] = inet6;
      }
    } else if (type == "local" || type == "dhcp") {
      if (type == "local") {
        server.remove("server");
        server.remove("server_port");
        server.remove("domain_resolver");
      }
      server.remove("detour");
    }
    return server;
  }

  const auto address = server["address"].toString();
  server.remove("address");
  if (address == "fakeip") {
    server["type"] = "fakeip";
    server.remove("domain_resolver");
    server.remove("detour");
    const auto inet4 = fakeip["inet4_range"].toString();
    const auto inet6 = fakeip["inet6_range"].toString();
    if (!inet4.isEmpty()) server["inet4_range"] = inet4;
    if (!inet6.isEmpty()) server["inet6_range"] = inet6;
    return server;
  }
  if (address == "local" || address.isEmpty()) {
    server["type"] = "local";
    server.remove("detour");
    return server;
  }
  if (server["detour"].toString() == "direct") {
    server["type"] = "local";
    server.remove("detour");
    return server;
  }

  QUrl url(address);
  const auto scheme = url.scheme().toLower();
  if (scheme == "tcp" || scheme == "tls" || scheme == "quic" ||
      scheme == "https" || scheme == "h3") {
    server["type"] = scheme;
    server["server"] = url.host().isEmpty() ? address.mid(scheme.size() + 3)
                                            : url.host();
    if (url.port() > 0) server["server_port"] = url.port();
    if (!url.path().isEmpty() && url.path() != "/") server["path"] = url.path();
    return server;
  }
  if (scheme == "dhcp") {
    server["type"] = "dhcp";
    server.remove("detour");
    const auto host = url.host();
    if (!host.isEmpty() && host != "auto") server["interface"] = host;
    return server;
  }

  server["type"] = "udp";
  server["server"] = address;
  return server;
}

static QJsonObject convertV2RayNToSingBox(const QJsonObject &v2rayn) {
  QJsonObject result;
  auto protocol = v2rayn["protocol"].toString();
  auto tag = v2rayn["tag"].toString();
  result["type"] = protocol;
  result["tag"] = tag;

  if (protocol == "freedom" || protocol == "blackhole" || protocol == "dns" ||
      protocol == "wireguard" || protocol == "shadowsocks") {
    if (protocol == "freedom")     result["type"] = "direct";
    if (protocol == "blackhole") result["type"] = "block";
    if (protocol == "dns") return QJsonObject();
    if (protocol == "shadowsocks") {
      auto settings = v2rayn["settings"].toObject();
      auto servers = settings["servers"].toArray();
      if (!servers.isEmpty()) {
        auto server = servers[0].toObject();
        result["method"] = server["method"].toString();
        result["password"] = server["password"].toString();
        result["server"] = server["address"].toString();
        result["server_port"] = server["port"].toInt();
      }
    }
    return result;
  }

  auto settings = v2rayn["settings"].toObject();
  auto streamSettings = v2rayn["streamSettings"].toObject();
  auto hysteriaSettings = streamSettings["hysteriaSettings"].toObject();
  if (protocol == "hysteria") {
    const auto version = jsonFirstInt(settings, {"version"});
    const auto streamVersion = jsonFirstInt(hysteriaSettings, {"version"});
    if (version == 2 || streamVersion == 2) {
      protocol = "hysteria2";
      result["type"] = protocol;
    }
  }
  auto vnext = settings["vnext"].toArray();
  auto servers = settings["servers"].toArray();
  if (!vnext.isEmpty()) {
    auto server = vnext[0].toObject();
    result["server"] = server["address"].toString();
    result["server_port"] = server["port"].toInt();
    auto users = server["users"].toArray();
    if (!users.isEmpty()) {
      auto user = users[0].toObject();
      if (protocol == "vless") {
        result["uuid"] = user["id"].toString();
        auto flow = user["flow"].toString();
        if (!flow.isEmpty()) result["flow"] = flow;
        result["encryption"] = user["encryption"].toString();
      } else if (protocol == "vmess") {
        result["uuid"] = user["id"].toString();
        result["alter_id"] = user["aid"].toInt();
        result["security"] = user["security"].toString();
      } else if (protocol == "trojan") {
        result["password"] = user["password"].toString();
      }
    }
  } else if (!servers.isEmpty()) {
    auto server = servers[0].toObject();
    if (protocol == "shadowsocks") {
      result["method"] = server["method"].toString();
      result["password"] = server["password"].toString();
      result["server"] = server["address"].toString();
      result["server_port"] = server["port"].toInt();
    } else if (protocol == "hysteria" || protocol == "hysteria2" ||
               protocol == "tuic") {
      auto address = jsonFirstString(server, {"address", "server", "host"});
      if (address.isEmpty()) {
        address = jsonFirstString(settings, {"address", "server", "host"});
      }
      auto port = jsonFirstInt(server, {"port", "server_port", "serverPort"});
      if (port <= 0) {
        port = jsonFirstInt(settings, {"port", "server_port", "serverPort"});
      }
      if (!address.isEmpty()) result["server"] = address;
      if (port > 0) result["server_port"] = port;

      auto upMbps = jsonFirstInt(server, {"up_mbps", "upMbps", "up", "upmbps"});
      if (upMbps <= 0) {
        upMbps = jsonFirstInt(settings, {"up_mbps", "upMbps", "up", "upmbps"});
      }
      auto downMbps =
          jsonFirstInt(server, {"down_mbps", "downMbps", "down", "downmbps"});
      if (downMbps <= 0) {
        downMbps =
            jsonFirstInt(settings, {"down_mbps", "downMbps", "down", "downmbps"});
      }
      if (upMbps > 0) result["up_mbps"] = upMbps;
      if (downMbps > 0) result["down_mbps"] = downMbps;

      if (protocol == "hysteria") {
        auto hy2Password = jsonFirstString(server, {"password"});
        if (hy2Password.isEmpty()) {
          hy2Password = jsonFirstString(settings, {"password"});
        }
        if (hy2Password.isEmpty()) {
          hy2Password = jsonFirstString(hysteriaSettings, {"password", "auth"});
        }
        auto auth = jsonFirstString(server, {"auth", "auth_str", "auth-str"});
        if (auth.isEmpty()) {
          auth = jsonFirstString(settings, {"auth", "auth_str", "auth-str"});
        }
        if (auth.isEmpty()) {
          auth = jsonFirstString(hysteriaSettings, {"auth", "auth_str", "auth-str"});
        }
        if (!hy2Password.isEmpty() && auth.isEmpty()) {
          result["type"] = "hysteria2";
          result["password"] = hy2Password;
        } else if (!auth.isEmpty()) {
          result["auth_str"] = auth;
        }
        auto obfs = jsonFirstString(server, {"obfs", "obfsPassword", "obfs-password"});
        if (obfs.isEmpty()) {
          obfs = jsonFirstString(settings, {"obfs", "obfsPassword", "obfs-password"});
        }
        if (!obfs.isEmpty()) {
          if (result["type"].toString() == "hysteria2") {
            result["obfs"] = QJsonObject{
                {"type", "salamander"},
                {"password", obfs},
            };
          } else {
            result["obfs"] = obfs;
          }
        }
      } else if (protocol == "hysteria2") {
        auto password = jsonFirstString(server, {"password", "auth"});
        if (password.isEmpty()) {
          password = jsonFirstString(settings, {"password", "auth"});
        }
        if (password.isEmpty()) {
          password = jsonFirstString(hysteriaSettings, {"password", "auth"});
        }
        if (!password.isEmpty()) result["password"] = password;
        auto obfs = jsonFirstString(server, {"obfs-password", "obfsPassword", "obfs"});
        if (obfs.isEmpty()) {
          obfs = jsonFirstString(settings, {"obfs-password", "obfsPassword", "obfs"});
        }
        if (!obfs.isEmpty()) {
          result["obfs"] = QJsonObject{
              {"type", "salamander"},
              {"password", obfs},
          };
        }
      } else if (protocol == "tuic") {
        auto uuid = jsonFirstString(server, {"uuid", "id"});
        if (uuid.isEmpty()) uuid = jsonFirstString(settings, {"uuid", "id"});
        auto password = jsonFirstString(server, {"password"});
        if (password.isEmpty()) password = jsonFirstString(settings, {"password"});
        if (!uuid.isEmpty()) result["uuid"] = uuid;
        if (!password.isEmpty()) result["password"] = password;
      }
    }
  }

  if ((protocol == "hysteria" || protocol == "hysteria2" ||
       protocol == "tuic") &&
      result["server"].toString().isEmpty()) {
    auto address = jsonFirstString(settings, {"address", "server", "host"});
    auto port = jsonFirstInt(settings, {"port", "server_port", "serverPort"});
    if (!address.isEmpty()) result["server"] = address;
    if (port > 0) result["server_port"] = port;
  }
  if (protocol == "hysteria" && result["auth"].toString().isEmpty() &&
      result["auth_str"].toString().isEmpty()) {
    auto auth = jsonFirstString(settings, {"auth", "auth_str", "auth-str"});
    if (auth.isEmpty()) {
      auth = jsonFirstString(hysteriaSettings, {"auth", "auth_str", "auth-str"});
    }
    if (!auth.isEmpty()) result["auth_str"] = auth;
  } else if (protocol == "hysteria2" &&
             result["password"].toString().isEmpty()) {
    auto password = jsonFirstString(settings, {"password", "auth"});
    if (password.isEmpty()) {
      password = jsonFirstString(hysteriaSettings, {"password", "auth"});
    }
    if (!password.isEmpty()) result["password"] = password;
  }

  auto security = streamSettings["security"].toString();
  auto network = streamSettings["network"].toString();
  if (network.isEmpty()) network = "tcp";

  if (security == "tls" || security == "reality" || security == "xtls") {
    QJsonObject tls;
    tls["enabled"] = true;
    auto sni = streamSettings["sni"].toString();
    if (sni.isEmpty()) {
      auto sObj = streamSettings["tlsSettings"].toObject();
      sni = sObj["serverName"].toString();
    }
    tls["server_name"] = sni;
    auto fp = streamSettings["fingerprint"].toString();
    if (fp.isEmpty()) {
      auto sObj = streamSettings["tlsSettings"].toObject();
      fp = sObj["fingerprint"].toString();
    }
    if (security == "reality") {
      QJsonObject reality;
      reality["enabled"] = true;
      auto rs = streamSettings["realitySettings"].toObject();
      reality["public_key"] = rs["publicKey"].toString();
      reality["short_id"] = rs["shortId"].toString();
      auto rSni = rs["serverName"].toString();
      if (!rSni.isEmpty()) tls["server_name"] = rSni;
      if (fp.isEmpty()) fp = rs["fingerprint"].toString();
      tls["reality"] = reality;
      if (!fp.isEmpty()) {
        QJsonObject utls;
        utls["enabled"] = true;
        utls["fingerprint"] = fp;
        tls["utls"] = utls;
      }
    } else {
      if (!fp.isEmpty()) {
        QJsonObject utls;
        utls["enabled"] = true;
        utls["fingerprint"] = fp;
        tls["utls"] = utls;
      }
      auto alpn = streamSettings["alpn"].toString();
      auto tlsAlpn = streamSettings["tlsSettings"].toObject()["alpn"].toArray();
      if (!tlsAlpn.isEmpty()) {
        tls["alpn"] = tlsAlpn;
      } else if (!alpn.isEmpty()) {
        tls["alpn"] = QJsonArray{alpn};
      }
    }
    result["tls"] = tls;
  }

  if (protocol == "hysteria" || protocol == "hysteria2" || protocol == "tuic") {
    return result;
  }

  QJsonObject transport;
  transport["type"] = network;
  if (network == "ws") {
    auto ws = streamSettings["wsSettings"].toObject();
    transport["path"] = ws["path"].toString();
    auto headers = ws["headers"].toObject();
    transport["host"] = headers["Host"].toString();
  } else if (network == "grpc") {
    auto grpc = streamSettings["grpcSettings"].toObject();
    transport["service_name"] = grpc["serviceName"].toString();
  } else if (network == "h2" || network == "http") {
    auto h2 = streamSettings["httpSettings"].toObject();
    transport["path"] = h2["path"].toString();
    auto hosts = h2["host"].toArray();
    if (!hosts.isEmpty()) transport["host"] = hosts[0].toString();
  } else if (network == "httpupgrade") {
    auto s = streamSettings["httpupgradeSettings"].toObject();
    transport["path"] = s["path"].toString();
    transport["host"] = s["host"].toString();
  } else if (network == "splithttp") {
    auto s = streamSettings["splithttpSettings"].toObject();
    transport["path"] = s["path"].toString();
    transport["host"] = s["host"].toString();
  } else if (network == "xhttp") {
    auto s = streamSettings["xhttpSettings"].toObject();
    transport["path"] = s["path"].toString();
    transport["host"] = s["host"].toString();
    transport["mode"] = s["mode"].toString();
  }
  result["transport"] = transport;
  return result;
}

static void normalizeHysteriaOutbound(QJsonObject &out) {
  const auto type = out["type"].toString();
  if (type == "hysteria") {
    const bool hasUp = out.contains("up") || out.contains("up_mbps");
    const bool hasDown = out.contains("down") || out.contains("down_mbps");
    if (!hasUp) out["up_mbps"] = 100;
    if (!hasDown) out["down_mbps"] = 100;
  }
  if (type == "hysteria" || type == "hysteria2") {
    auto tls = out["tls"].toObject();
    if (tls.isEmpty()) {
      tls["enabled"] = true;
    }
    if (tls["server_name"].toString().isEmpty()) {
      tls["server_name"] = out["server"].toString();
    }
    tls.remove("utls");
    out["tls"] = tls;
  } else if (type == "tuic") {
    auto tls = out["tls"].toObject();
    if (!tls.isEmpty()) {
      tls.remove("utls");
      out["tls"] = tls;
    }
  }
}

static QJsonObject sanitizeSingBoxConfig(const QJsonObject &json) {
  auto result = json;
  result.remove("remarks");
  auto dns = result["dns"].toObject();
  dns.remove("queryStrategy");
  auto fakeip = dns["fakeip"].toObject();
  if (fakeip.isEmpty()) {
    fakeip = QJsonObject{
        {"inet4_range", "198.18.0.0/15"},
    };
  }
  auto servers = dns["servers"].toArray();
  QJsonArray cleanServers;
  QJsonArray dnsRules;
  QJsonObject fakeDnsSrv;
  int tagIdx = 0;
  QString fallbackDnsTag;
  for (auto s : servers) {
    if (s.isString()) {
      auto str = s.toString();
      if (str == "fakedns") {
        fakeDnsSrv["tag"] = "fakedns";
        fakeDnsSrv["type"] = "fakeip";
      } else {
        QJsonObject srv;
        srv["tag"] = "dns-" + QString::number(tagIdx++);
        srv["address"] = str;
        if (fallbackDnsTag.isEmpty()) fallbackDnsTag = srv["tag"].toString();
        cleanServers.append(migrateDnsServerToSingBox112(srv, fakeip));
      }
    } else if (s.isObject()) {
      auto obj = s.toObject();
      auto tag = obj.contains("tag") ? obj["tag"].toString() : "dns-" + QString::number(tagIdx++);
      auto domains = obj["domains"].toArray();
      obj["tag"] = tag;
      obj.remove("domains");
      if (fallbackDnsTag.isEmpty()) fallbackDnsTag = tag;
      cleanServers.append(migrateDnsServerToSingBox112(obj, fakeip));
      if (!domains.isEmpty()) {
        QJsonObject rule;
        rule["domain"] = domains;
        rule["server"] = tag;
        dnsRules.append(rule);
      }
    }
  }
  if (!fakeDnsSrv.isEmpty()) cleanServers.append(migrateDnsServerToSingBox112(fakeDnsSrv, fakeip));
  dns["servers"] = cleanServers;
  if (!dnsRules.isEmpty()) {
    auto existingRules = dns["rules"].toArray();
    for (auto r : existingRules) dnsRules.append(r);
    dns["rules"] = dnsRules;
  }
  auto fakeIp = result["fakedns"].toArray();
  if (!fakeIp.isEmpty() && fakeDnsSrv.isEmpty()) {
    QJsonObject fakeServer;
    fakeServer["tag"] = "fakedns";
    fakeServer["type"] = "fakeip";
    fakeServer["inet4_range"] = fakeip["inet4_range"].toString("198.18.0.0/15");
    auto serversWithFake = dns["servers"].toArray();
    serversWithFake.append(fakeServer);
    dns["servers"] = serversWithFake;
  }
  result.remove("fakedns");
  dns.remove("fakeip");
  result["dns"] = dns;
  auto outbounds = result["outbounds"].toArray();
  QJsonArray cleanOutbounds;
  for (auto o : outbounds) {
    if (!o.isObject()) continue;
    auto out = o.toObject();
    if (out.contains("protocol") && !out.contains("type")) {
      out = convertV2RayNToSingBox(out);
    }
    cleanOutbounds.append(out);
  }
  result["outbounds"] = cleanOutbounds;
  result["inbounds"] = QJsonArray();
  QJsonArray cleanObs;
  QString defaultOutboundTag;
  for (auto o : result["outbounds"].toArray()) {
    if (!o.isObject()) { continue; }
    auto out = o.toObject();
    if (out.isEmpty()) continue;
    QStringList keysToRemove;
    for (auto key : out.keys()) {
      if (out[key].isNull() || out[key].isUndefined()) {
        keysToRemove.append(key);
      }
    }
    for (auto &k : keysToRemove) out.remove(k);
    auto tr = out["transport"].toObject();
    auto type = out["type"].toString();
    if (type == "hysteria" || type == "hysteria2" || type == "tuic" ||
        tr.isEmpty() || (tr["type"].toString() == "tcp" && tr.size() <= 1)) {
      out.remove("transport");
    }
    normalizeHysteriaOutbound(out);
    if (defaultOutboundTag.isEmpty() && type != "direct" && type != "block" &&
        type != "dns") {
      auto tag = out["tag"].toString();
      if (tag.isEmpty()) {
        tag = "proxy";
        out["tag"] = tag;
      }
      defaultOutboundTag = tag;
    }
    cleanObs.append(out);
  }
  result["outbounds"] = cleanObs;
  const bool hasV2RayNRouting = result.contains("routing");
  auto routing =
      hasV2RayNRouting ? result["routing"].toObject() : result["route"].toObject();
  auto rules = routing["rules"].toArray();
  QJsonArray cleanRules;
  for (auto r : rules) {
    if (!r.isObject()) continue;
    auto rule = r.toObject();
    if (hasV2RayNRouting || rule.contains("outboundTag")) {
      auto outTag = rule["outboundTag"].toString();
      if (outTag == "dns-out") continue;
      rule.remove("type");
      rule.remove("outboundTag");
      rule["action"] = "route";
      if (!outTag.isEmpty()) rule["outbound"] = outTag;
      auto inTag = rule["inboundTag"].toArray();
      rule.remove("inboundTag");
      if (!inTag.isEmpty()) {
        QJsonArray inArr;
        for (auto t : inTag) inArr.append(t.toString());
        rule["inbound"] = inArr;
      }
    }
    QStringList ruleKeysToRemove;
    for (auto key : rule.keys()) {
      if (rule[key].isNull() || rule[key].isUndefined()) {
        ruleKeysToRemove.append(key);
      }
    }
    for (auto &k : ruleKeysToRemove) rule.remove(k);
    auto domains = rule["domain"].toArray();
    if (!domains.isEmpty()) {
      QJsonArray domSuffix;
      QJsonArray domFull;
      for (auto d : domains) {
        auto ds = d.toString();
        if (ds.startsWith("domain:")) {
          auto val = ds.mid(7);
          domSuffix.append(val);
        } else if (ds.startsWith("full:")) {
          domFull.append(ds.mid(5));
        } else {
          domFull.append(ds);
        }
      }
      rule.remove("domain");
      if (!domSuffix.isEmpty()) rule["domain_suffix"] = domSuffix;
      if (!domFull.isEmpty()) rule["domain"] = domFull;
    }
    cleanRules.append(rule);
  }
  routing["rules"] = cleanRules;
  routing.remove("domainMatcher");
  routing.remove("domainStrategy");
  if (!defaultOutboundTag.isEmpty())
    routing["final"] = defaultOutboundTag;
  routing["auto_detect_interface"] = true;
  if (!fallbackDnsTag.isEmpty()) {
    QJsonObject defResolver;
    defResolver["server"] = fallbackDnsTag;
    routing["default_domain_resolver"] = defResolver;
  }
  result.remove("routing");
  result.remove("route");
  result["route"] = routing;
  auto dnsServer = result["dns"].toObject();
  auto dnsServers = dnsServer["servers"].toArray();
  QJsonArray finalDns;
  for (auto s : dnsServers) {
    auto srv = s.toObject();
    finalDns.append(srv);
  }
  dnsServer["servers"] = finalDns;
  result["dns"] = dnsServer;
  QJsonObject experimentalObj;
  QJsonObject clash_api;
  clash_api["default_mode"] = "";
  if (Configs::dataStore->core_box_clash_api > 0) {
    clash_api["external_controller"] = Configs::dataStore->core_box_clash_listen_addr +
        ":" + QString::number(Configs::dataStore->core_box_clash_api);
    clash_api["secret"] = Configs::dataStore->core_box_clash_api_secret;
    clash_api["external_ui"] = "dashboard";
  }
  experimentalObj["clash_api"] = clash_api;
  result["experimental"] = experimentalObj;
  return result;
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
  auto addFullJsonProxy = [&](const QJsonObject &json, const QString &remarks) {
    ent = Configs::ProfileManager::NewProxyEntity("custom");
    auto bean = ent->unlock(ent->CustomBean());
    bean->core = "internal-full";
    auto cleanJson = sanitizeSingBoxConfig(json);
    bean->config_simple =
        QString(QJsonDocument(cleanJson).toJson(QJsonDocument::Compact));
    auto obs = cleanJson["outbounds"].toArray();
    QString dp;
    for (const auto &outbound : obs) {
      auto ft = outbound.toObject();
      auto ot = ft["type"].toString().toUpper();
      if (ot.isEmpty() || ot == "DIRECT" || ot == "BLOCK" || ot == "DNS") {
        continue;
      }
      auto tr = ft["transport"].toObject()["type"].toString().toUpper();
      dp = ot;
      if (!tr.isEmpty() && tr != "TCP") dp += "/" + tr;
      dp += "/JSON";
      break;
    }
    if (!remarks.isEmpty()) ent->name = remarks;
    if (!dp.isEmpty()) ent->display_type = dp;
    AddProxy(ent);
  };

ret_loop:
  if (stack.size() != 0) {
    str = stack.takeFirst();
  } else {
    return;
  }

  if (str.startsWith("happ://")) {
    auto decrypted = HappDecrypt::decryptLink(str);
    if (!decrypted.isEmpty()) {
      MW_show_log("<<<<<<<< " + QObject::tr("Decrypted happ:// link"));
      str = decrypted;
    } else {
      MW_show_log("<<<<<<<< " + QObject::tr("Failed to decrypt happ:// link"));
      goto ret_loop;
    }
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


  int scheme_index ;
  QString scheme;

  {
    int slash_count = 0;
    int colon_index = -1;
    int index = 0;
    int count = str.size();
    if (count > 30){
        // there is no scheme name with a size greater than 27
        count = 30;
    }
    while (index < count){
      auto ch = str[index].unicode();
      if (colon_index >= 0){
        if (ch == '/'){
          slash_count ++;
          if (slash_count == 2){
            break;
          } else {
            index ++;
            continue;
          }
        } else {
          break;
        }
      }
      // check if letter
      if (QChar::isLetter(ch)){
          index ++;
          continue;
        } else {
          if (ch == ':'){
            colon_index = index;
            index ++;
            continue;
          } else {
            break;
          }
        }
    }
    if (colon_index > 0 && slash_count == 2){
      scheme_index = colon_index;
    } else {
      scheme_index = -1;
    }
  }

  if (scheme_index == -1) {
  jsonDocument = QJsonDocument::fromJson(str.toUtf8(), &error);
  json_ok = error.error == error.NoError;
  if (json_ok) {
    if (jsonDocument.isArray()) {
      for (auto i : jsonDocument.array()) {
        if (i.isObject()) {
          auto json = i.toObject();
          if (json.contains("proxy")) {
            stack << json["proxy"].toString();
          } else if (json.contains("outbounds") || json.contains("endpoints")) {
            auto remarks = json["remarks"].toString();
            if (json.contains("inbounds") || json.contains("routing") ||
                json.contains("route") || json.contains("dns")) {
              addFullJsonProxy(json, remarks);
            } else {
              updateSingBox(json, remarks);
            }
          }
        } else if (i.isString()) {
          stack << i.toString();
        }
      }
      goto ret_loop;
    } else if (!jsonDocument.isObject()) {
      goto ret_loop;
    } else {
      json = jsonDocument.object();
    }
    }
  } else {
    json_ok = false;
  }
  if (json_ok) {
    json_contains_endpoints = json.contains("endpoints");
    json_contains_outbounds = json.contains("outbounds");
    json_contains_inbounds = json.contains("inbounds");
    // SingBox
    if ((json_contains_outbounds || json_contains_endpoints)) {
      if (json_contains_inbounds || json.contains("routing") ||
          json.contains("route") || json.contains("dns")) {
        addFullJsonProxy(json, json["remarks"].toString());
      } else {
        updateSingBox(json);
      }
      goto ret_loop;
    }

    // SIP008
    if (json.contains("version") && json.contains("servers")) {
      updateSIP008(json);
      goto ret_loop;
    }
    goto parse_json;
  }


  if (scheme_index == -1){
    // Clash
    if (updateClash(str)) {
      goto ret_loop;
    }

   // Wireguard Config
    if (updateWireguardFileConfig(str)){
      goto ret_loop;
    }
  }

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

  if (str.startsWith("//") || str.startsWith("#") || str.length() < 2) {
    goto ret_loop;
  }

  if (scheme_index > 0) {
    scheme = str.sliced(0, scheme_index).toLower();
  } else {
    goto ret_loop;
  }

  bool quic_enabled = false;

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
  else if (scheme == "wg"
        || scheme == "awg"
        || scheme == "wireguard"
        || scheme == "amneziawg"
        || scheme == "amnezia") {
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

void RawUpdater::updateSingBox(const QJsonObject &json, const QString &name) {
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

    if (out_type.isEmpty() && !out["protocol"].toString().isEmpty()) {
      out = convertV2RayNToSingBox(out);
      out_type = out["type"].toString();
    }

    // All Types
    if (Preset::SingBox::OutboundTypes.contains(out_type)) {
      ent = Configs::ProfileManager::NewProxyEntity(out_type);
      auto ok = ent->unlock(ent->bean())->TryParseJson(out);
      if (!ok)
        continue;
    }

    if (ent == nullptr)
      continue;

    if (!name.isEmpty() && items.size() == 1) {
      ent->name = name;
    }

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

void GroupUpdater::AsyncUpdateGroup(
    std::shared_ptr<Configs::Group> group,
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    const std::function<QString(bool*,bool*,const QString&)> &info,
    const std::function<void()> &finish,
    const std::function<std::shared_ptr<const Configs::GroupExtra>(
        std::shared_ptr<const Configs::GroupExtra>)> &updater
){
    if (group->is_subscription){
        bool send_hwid;
        QString custom_hwid;
        auto extra = group->getExtra();
        bool custom_payload = extra->enable_custom_payload;
        if (custom_payload && updater != nullptr){
            auto & javascript = extra->javascript_payload;
            if (!javascript.isEmpty()){
                extra = updater(extra);
            }
        }
        QByteArray payload = {};
        bool headers_enabled;
        if ((headers_enabled = extra->enable_custom_headers)){
            send_hwid = extra->enable_hwid;
            custom_hwid = extra->custom_hwid;
        } else {
            send_hwid = Configs::dataStore->sub_send_hwid;
            custom_hwid = Configs::dataStore->sub_custom_hwid_params;
        }
        QMap<QString, QString> headers;
        if (send_hwid) headers = Configs_network::GetHWID(custom_hwid);
        if (headers_enabled){
            for (auto [key, value] : asKeyValueRange(extra->custom_headers)){
                headers.insert(key, value.toString());
            }
        }
        if (custom_payload){
            payload = extra->text_payload.toUtf8();
        }
        AsyncUpdate(PreFinishJob, extra->url, info, group->id, finish, headers, payload, true);
    }
};

//
void GroupUpdater::AsyncUpdate(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    const QString &str,
    const std::function<QString(bool *, bool *, const QString &)> &info,
    int _sub_gid, const std::function<void()> &finish,
    const QMap<QString, QString> &headers,
    const QByteArray & array, bool no_hwid) {
  auto content = str.trimmed();
  bool asURL = false;
  bool createNewGroup = false;
  QString groupName = "";

  if (_sub_gid < 0 &&
      (content.startsWith("http://") || content.startsWith("https://") || content.startsWith("happ://"))) {
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
      group->is_subscription = true;
      Configs::profileManager->AddGroup(group);
      auto extra = group->getExtraUnlocked();

      extra->url = str;
      gid = group->id;
      MW_dialog_message("SubUpdater", "NewGroup");
    }
    Update(PreFinishJob, str, gid, asURL, createNewGroup && groupName.isEmpty(),
           headers, array, no_hwid);
    emit asyncUpdateCallback(gid);
    if (finish != nullptr)
      finish();
  });
}

/*

*/

void GroupUpdater::Update(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    const QString &_str, int _sub_gid, bool _not_sub_as_url, bool _auto_name,
    const QMap<QString, QString> &headers,
    const QByteArray & array, bool no_hwid) {
  //
  Configs::dataStore->imported_count = 0;
  auto rawUpdater = std::make_unique<RawUpdater>();
  rawUpdater->gid_add_to = _sub_gid;

  //
  QString sub_user_info;
  bool asURL = _sub_gid >= 0 || _not_sub_as_url;
  auto content = _str.trimmed();

  if (content.startsWith("happ://")) {
    auto decrypted = HappDecrypt::decryptLink(content);
    if (!decrypted.isEmpty()) {
      MW_show_log("<<<<<<<< " + QObject::tr("Decrypted happ:// link"));
      content = decrypted;
      asURL = true;
    } else {
      MW_show_log("<<<<<<<< " + QObject::tr("Failed to decrypt happ:// link"));
      return;
    }
  }

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
        content, Configs::dataStore->sub_send_hwid && (!no_hwid), headers, array);
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

    group->Save();
    auto extra = group->getExtraUnlocked();
    extra->sub_last_update = QDateTime::currentMSecsSinceEpoch() / 1000;
    extra->info = sub_user_info;
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
  (g == nullptr || !g->is_subscription || g->archive ||                           \
   (onlyAllowed && g->getExtra()->skip_auto_update))

void serialUpdateSubscription(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    const QList<int> &groupsTabOrder,
    const std::function<QString(bool *, bool *, const QString &)> &info,
    int _order, bool onlyAllowed,     const std::function<std::shared_ptr<const Configs::GroupExtra>(
            std::shared_ptr<const Configs::GroupExtra>)> &updater) {
  if (_order >= groupsTabOrder.size()) {
    UI_update_all_groups_Updating = false;
    return;
  }

  // calculate this group
  auto group = Configs::profileManager->GetGroup(groupsTabOrder[_order]);
  if (group == nullptr || should_skip_group(group)) {
    serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, _order + 1,
                             onlyAllowed, updater);
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
  Subscription::groupUpdater->AsyncUpdateGroup( group,
      PreFinishJob, info, [=] {
        serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, nextOrder,
                                 onlyAllowed, updater);
      }, updater);
}

void UI_update_all_groups(
    const std::function<void(std::shared_ptr<Configs::Group>)> PreFinishJob,
    bool onlyAllowed,
    const std::function<QString(bool *, bool *, const QString &)> &info,
    const std::function<std::shared_ptr<const Configs::GroupExtra>(
            std::shared_ptr<const Configs::GroupExtra>)> &updater
        ) {
  if (UI_update_all_groups_Updating) {
    MW_show_log("The last subscription update has not exited.");
    return;
  }

  auto groupsTabOrder = Configs::profileManager->groupsTabOrder;
  serialUpdateSubscription(PreFinishJob, groupsTabOrder, info, 0, onlyAllowed, updater);
}
