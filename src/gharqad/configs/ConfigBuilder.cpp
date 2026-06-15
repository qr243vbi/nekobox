

#include <nekobox/dataStore/RouteEntity.h>
#include <nekobox/dataStore/Utils.hpp>

#include <nekobox/api/RPC.h>
#include <nekobox/configs/ConfigBuilder.hpp>
#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/DataStore.hpp>
#include <nekobox/dataStore/Database.hpp>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace Configs {

static QString ResolveDomainForTestConfig(const QString &server) {
  if (server.isEmpty() || IsIpAddress(server)) {
    return {};
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo *result = nullptr;
  if (getaddrinfo(server.toUtf8().constData(), nullptr, &hints, &result) != 0) {
    return {};
  }

  QString ip;
  char buffer[INET6_ADDRSTRLEN] = {};
  for (auto *p = result; p != nullptr; p = p->ai_next) {
    if (p->ai_family == AF_INET) {
      auto *addr = reinterpret_cast<sockaddr_in *>(p->ai_addr);
      if (inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer)) !=
          nullptr) {
        ip = QString::fromLatin1(buffer);
        break;
      }
    } else if (p->ai_family == AF_INET6) {
      auto *addr = reinterpret_cast<sockaddr_in6 *>(p->ai_addr);
      if (inet_ntop(AF_INET6, &addr->sin6_addr, buffer, sizeof(buffer)) !=
          nullptr) {
        ip = QString::fromLatin1(buffer);
        break;
      }
    }
  }

  freeaddrinfo(result);
  return ip;
}

static void ResolveOutboundServerForTestConfig(QJsonObject &outbound) {
  const auto server = outbound["server"].toString();
  const auto ip = ResolveDomainForTestConfig(server);
  if (ip.isEmpty()) {
    return;
  }

  outbound["server"] = ip;

  auto tls = outbound["tls"].toObject();
  if (!tls.isEmpty() && tls["server_name"].toString().isEmpty()) {
    tls["server_name"] = server;
    outbound["tls"] = tls;
  }
}

static void NormalizeFullConfigDnsForRuntime(QJsonObject &config) {
  auto dns = config["dns"].toObject();
  auto servers = dns["servers"].toArray();
  QJsonArray cleanServers;
  QString firstDnsTag;
  auto remoteDnsAddress = dataStore->routing->remote_dns;
  if (remoteDnsAddress.isEmpty() || remoteDnsAddress.startsWith("local") ||
      remoteDnsAddress == "localhost") {
    remoteDnsAddress = "tls://8.8.8.8";
  }
  for (auto serverRef : servers) {
    if (!serverRef.isObject()) {
      cleanServers.append(serverRef);
      continue;
    }
    auto server = serverRef.toObject();
    auto tag = server["tag"].toString();
    if (tag.isEmpty()) {
      tag = "dns-" + QString::number(cleanServers.size());
    }
    server.remove("address_resolver");
    const auto type = server["type"].toString();
    const auto address = server["address"].toString();
    if (type == "fakeip" || address == "fakeip") {
      server["tag"] = tag;
      server["type"] = "fakeip";
      server.remove("address");
      server.remove("domain_resolver");
      server.remove("detour");
    } else {
      server = BuildDnsObject(remoteDnsAddress, true);
      server["tag"] = tag;
      server["detour"] = "proxy";
      if (!IsIpAddress(server["server"].toString())) {
        server["domain_resolver"] = "dns-local";
      }
      if (firstDnsTag.isEmpty()) firstDnsTag = tag;
    }
    cleanServers.append(server);
  }
  {
    bool hasLocal = false;
    for (const auto &serverRef : cleanServers) {
      const auto server = serverRef.toObject();
      if (server["tag"].toString() == "dns-local") {
        hasLocal = true;
        break;
      }
    }
    if (!hasLocal) {
      QJsonObject dnsLocalObj = BuildDnsObject("local", true);
      dnsLocalObj["tag"] = "dns-local";
      cleanServers.append(dnsLocalObj);
    }
    if (!firstDnsTag.isEmpty()) {
      dns["final"] = firstDnsTag;
      auto route = config["route"].toObject();
      route["default_domain_resolver"] = QJsonObject{{"server", firstDnsTag}};
      config["route"] = route;
    }
  }
  if (!cleanServers.isEmpty()) {
    dns["servers"] = cleanServers;
    config["dns"] = dns;
  }
}

static void NormalizeFullConfigOutboundForRuntime(QJsonObject &outbound) {
  const auto type = outbound["type"].toString();
  if (type == "hysteria") {
    const bool hasUp = outbound.contains("up") || outbound.contains("up_mbps");
    const bool hasDown =
        outbound.contains("down") || outbound.contains("down_mbps");
    if (!hasUp) outbound["up_mbps"] = 100;
    if (!hasDown) outbound["down_mbps"] = 100;
  }
  if (type == "hysteria" || type == "hysteria2") {
    auto tls = outbound["tls"].toObject();
    if (tls.isEmpty()) {
      tls["enabled"] = true;
    }
    if (tls["server_name"].toString().isEmpty()) {
      tls["server_name"] = outbound["server"].toString();
    }
    tls.remove("utls");
    outbound["tls"] = tls;
  }
  if (type == "tuic") {
    auto tls = outbound["tls"].toObject();
    if (!tls.isEmpty()) {
      tls.remove("utls");
      outbound["tls"] = tls;
    }
  }
}

static void ResolveFullConfigOutboundServersForRuntime(QJsonObject &config) {
  auto resolveArray = [](QJsonArray outbounds) {
    QJsonArray resolved;
    for (auto outboundRef : outbounds) {
      auto outbound = outboundRef.toObject();
      NormalizeFullConfigOutboundForRuntime(outbound);
      const auto type = outbound["type"].toString();
      if (type != "direct" && type != "block" && type != "dns") {
        ResolveOutboundServerForTestConfig(outbound);
      }
      resolved.append(outbound);
    }
    return resolved;
  };
  config["outbounds"] = resolveArray(config["outbounds"].toArray());
  if (config.contains("endpoints")) {
    config["endpoints"] = resolveArray(config["endpoints"].toArray());
  }
}

static QString ToTunRouteExclude(const QString &server) {
  auto address = server;
  if (address.isEmpty()) {
    return {};
  }
  if (!IsIpAddress(address)) {
    address = ResolveDomainForTestConfig(address);
  }
  if (address.isEmpty()) {
    return {};
  }
  return IsIpAddressV6(address) ? address + "/128" : address + "/32";
}

static void CollectTunRouteExcludesFromArray(const QJsonArray &outbounds,
                                             QStringList &excludes) {
  for (const auto &value : outbounds) {
    const auto outbound = value.toObject();
    const auto type = outbound["type"].toString();
    if (type == "direct" || type == "block" || type == "dns") {
      continue;
    }
    const auto exclude = ToTunRouteExclude(outbound["server"].toString());
    if (!exclude.isEmpty() && !excludes.contains(exclude)) {
      excludes += exclude;
    }
  }
}

static QStringList CollectTunRouteExcludesForFullConfig(
    const QJsonObject &config) {
  QStringList excludes;
  CollectTunRouteExcludesFromArray(config["outbounds"].toArray(), excludes);
  CollectTunRouteExcludesFromArray(config["endpoints"].toArray(), excludes);
  return excludes;
}

static QString TunDnsAddress() {
  auto address = getTunAddress().section('/', 0, 0);
  auto parts = address.split('.');
  if (parts.size() == 4) {
    parts[3] = "2";
    return parts.join('.');
  }
  return "172.19.0.2";
}

static void AddTunRuntimeRules(QJsonObject &config,
                               const QStringList &serverExcludes,
                               const QJsonArray &nekoboxRules = {}) {
  auto route = config["route"].toObject();
  auto rules = route["rules"].toArray();

  QJsonArray patchedRules;
  if (!serverExcludes.isEmpty()) {
    patchedRules += QJsonObject{
        {"action", "route"},
        {"ip_cidr", QListStr2QJsonArray(serverExcludes)},
        {"outbound", "direct"},
    };
  }
  patchedRules += QJsonObject{
      {"action", "hijack-dns"},
      {"ip_cidr", QJsonArray{TunDnsAddress() + "/32"}},
      {"port", 53},
      {"inbound", QJsonArray{"tun-in"}},
  };
  patchedRules += QJsonObject{
      {"action", "hijack-dns"},
      {"protocol", "dns"},
      {"inbound", QJsonArray{"tun-in"}},
  };
  if (!dataStore->routing->domain_strategy.isEmpty()) {
    patchedRules += QJsonObject{
        {"action", "resolve"},
        {"inbound", QJsonArray{"tun-in"}},
        {"strategy", dataStore->routing->domain_strategy},
    };
  }
  patchedRules += QJsonObject{
      {"action", "sniff"},
      {"inbound", QJsonArray{"tun-in"}},
  };
  for (const auto &rule : rules) {
    patchedRules += rule;
  }
  for (const auto &rule : nekoboxRules) {
    patchedRules += rule;
  }

  route["auto_detect_interface"] = true;
  route["rules"] = patchedRules;
  config["route"] = route;
}

QString get_rule_set_name_1(const QString &ruleSet) {
  QUrl url;
  QString filename;
  if ((url = QUrl(ruleSet), url.isValid()) &&
      (filename = url.fileName(),
       (filename.endsWith(".srs")) || (filename.endsWith(".json")))) {
    return filename.replace(".", "-") + "-" +
           QString::number(qHash(url.toEncoded()));
  } else {
    return ruleSet;
  }
}

QJsonObject get_rule_set_json(const QString &ruleSet) {
  QString filename;
  QString format;
  QUrl url;

  bool reset = false;

  bool json = false;
  bool binary = false;

  QString strUrl;

label1:

  if ((url = (reset ? QUrl(strUrl) : QUrl(ruleSet)), url.isValid()) &&
      (filename = url.fileName(), (binary = filename.endsWith(".srs")) ||
                                      (json = filename.endsWith(".json")))) {
    QString tag;
    if (reset) {
      tag = ruleSet;
    } else {
      tag = filename.replace(".", "-") + "-" +
            QString::number(qHash(url.toEncoded()));
    }
    if (json) {
      format = "source";
    } else if (binary) {
      format = "binary";
    }
    /*
                QFileInfo CachePath (get_cache_from_url(url));
                if (CachePath.exists() || CachePath.isFile()){
                    return QJsonObject{
                        {"type", "local"},
                        {"format", format},
                        {"tag", tag},
                        {"path", CachePath.absoluteFilePath()}
                    };
                }
    */
    return QJsonObject{{"type", "remote"},
                       {"format", format},
                       {"tag", tag},
                       {"url", get_jsdelivr_link(url.toString())}};
  } else if (!reset) {

    auto iter = ruleSetMap.find(ruleSet);
    if (iter != ruleSetMap.end()) {
      reset = true;
      strUrl = iter.value().toString();
      goto label1;
    } else if (ruleSet == "nekobox-adblocksingbox") {
      reset = true;
      strUrl = "https://raw.githubusercontent.com/217heidai/adblockfilters/"
               "main/rules/adblocksingbox.srs";
      goto label1;
    }
  }
  return QJsonObject{};
}

static QJsonArray BuildNekoboxTunRulesForFullConfig(
    const std::shared_ptr<BuildConfigStatus> &status, QJsonObject &config);

QString getTunAddress() {
  if (Configs::dataStore->tun_address.isEmpty())
    return "172.19.0.1/24";
  return Configs::dataStore->tun_address;
}

QString getTunAddress6() {
  if (Configs::dataStore->tun_address_6.isEmpty())
    return "fdfe:dcba:9876::1/96";
  return Configs::dataStore->tun_address_6;
}

QString getTunName() {
  return "tun_" + GetRandomString(9, ExcludeUppercase | ExcludeDigits);
}

void MergeJson(const QJsonObject &custom, QJsonObject &outbound) {
  //
  if (custom.isEmpty())
    return;
  for (const auto &key : custom.keys()) {
    if (outbound.contains(key)) {
      auto v = custom[key];
      auto v_orig = outbound[key];
      if (v.isObject() && v_orig.isObject()) { //
        auto vo = v.toObject();
        QJsonObject vo_orig = v_orig.toObject();
        MergeJson(vo, vo_orig);
        outbound[key] = vo_orig;
      } else {
        outbound[key] = v;
      }
    } else {
      outbound[key] = custom[key];
    }
  }
}

// Common

std::shared_ptr<BuildConfigResult>
BuildConfig(const std::shared_ptr<ProxyEntity> &ent, bool forTest,
            bool forExport, int chainID) {
  auto result = std::make_shared<BuildConfigResult>();
  result->extraCoreData = std::make_shared<ExtraCoreData>();
  auto status = std::make_shared<BuildConfigStatus>();
  status->ent = ent;

  /*
  if (ent != nullptr && ent->bean->DisplayName() == "!!aaaa!!"){
      status->ent = nullptr;
  }
  */

  status->result = result;
  status->forTest = forTest;
  status->forExport = forExport;
  status->chainID = chainID;

  if (status->ent != nullptr && status->ent->type == "custom") {
    auto customBean = ent->CustomBean();
    if (customBean != nullptr && customBean->core == "internal-full") {
      result->coreConfig = QString2QJsonObject(customBean->config_simple);
      NormalizeFullConfigDnsForRuntime(result->coreConfig);
      ResolveFullConfigOutboundServersForRuntime(result->coreConfig);
      if (!status->forTest) {
        QJsonArray clientInbounds;
        if (IsValidPort(dataStore->inbound_socks_port) &&
            Configs::dataStore->proxyInboundEnabled()) {
          QJsonObject inboundObj;
          inboundObj["tag"] = "mixed-in";
          inboundObj["type"] =
#ifdef USE_CPP_PROXY_CONFIGURATOR
              "mixed"
#else
              (QString)*Configs::dataStore->inbound_proxy_type;
#endif
          inboundObj["listen"] = dataStore->inbound_address;
          inboundObj["listen_port"] = dataStore->inbound_socks_port;
          auto &uname = dataStore->inbound_username;
          auto &upass = dataStore->inbound_password;
          if (!uname.isEmpty() && !upass.isEmpty()) {
            QJsonArray users;
            QJsonObject user;
            user["username"] = uname;
            user["password"] = upass;
            users += user;
            inboundObj["users"] = users;
          }
#ifndef USE_CPP_PROXY_CONFIGURATOR
          inboundObj["set_system_proxy"] = Configs::dataStore->spmode_system_proxy;
#endif
          clientInbounds.append(inboundObj);
        }
        if (dataStore->spmode_vpn) {
          auto nekoboxTunRules =
              BuildNekoboxTunRulesForFullConfig(status, result->coreConfig);
          if (!status->result->error.isEmpty()) {
            return result;
          }
          auto tunServerExcludes =
              CollectTunRouteExcludesForFullConfig(result->coreConfig);
          clientInbounds.append(BuildTunInbound({}, tunServerExcludes));
          AddTunRuntimeRules(result->coreConfig, tunServerExcludes,
                             nekoboxTunRules);
        }
        if (!clientInbounds.isEmpty())
          result->coreConfig["inbounds"] = clientInbounds;
      }
    } else {
      BuildConfigSingBox(status);
    }

    // apply custom config
    MergeJson(QString2QJsonObject(customBean->custom_config),
              result->coreConfig);
  } else {
#ifdef DEBUG_MODE
    qDebug() << "Generating Block Network Config";
#endif
    BuildConfigSingBox(status);
  }

  return result;
}

bool IsValid(std::shared_ptr<ProxyEntity> ent) {
  if (ent == nullptr) {
    return false;
  }
  if (ent->type == "chain") {
    auto bean = ent->ChainBean();
    if (bean == nullptr) {
      return false;
    }
    for (int eId : bean->list) {
      auto e = profileManager->GetProfile(eId);
      if (e == nullptr) {
        MW_show_log("Null ent in validator: ID=" + QString::number(eId));
        return false;
      }
      if (!IsValid(e)) {
        MW_show_log("Invalid ent in chain: ID=" + QString::number(eId));
        return false;
      }
    }
    return true;
  }
  QJsonObject conf;
  auto bean = ent->bean();

  if (bean != nullptr && ent->type == "custom" &&
      ent->CustomBean()->core == "internal-full") {
    conf = QString2QJsonObject(ent->CustomBean()->config_simple);
  } else {
    auto out = bean->BuildCoreObjSingBox();
    auto outArr = QJsonArray{out.outbound};
    auto key = bean->IsEndpoint() ? "endpoints" : "outbounds";
    conf = {
        {key, outArr},
    };
  }
  bool ok;
  auto resp =
      API::defaultClient->CheckConfig(&ok, QJsonObject2QString(conf, true));
  if (!ok) {
    MW_show_log("Failed to contact the Core: " + resp);
    return false;
  }
  if (resp.isEmpty())
    return true;
  // else
  MW_show_log("Invalid ent " + ent->name + ": " + resp);
  return false;
}

std::shared_ptr<BuildTestConfigResult>
BuildTestConfig(const QList<std::shared_ptr<ProxyEntity>> &profiles) {
  auto results = std::make_shared<BuildTestConfigResult>();

  QJsonArray outboundArray = {
      QJsonObject{{"type", "direct"}, {"tag", "direct"}}};
  QJsonArray endpointArray = {};
  int index = 0;

  QJsonArray directDomainArray;
  for (const auto &item : profiles) {
    if (item == nullptr) {
      continue;
    }
    auto bean = item->bean();
    if (bean == nullptr) {
      continue;
    }
    if (item->type == "extracore") {
      MW_show_log("Skipping ExtraCore conf");
      continue;
    }
    if (!IsValid(item)) {
      MW_show_log("Skipping invalid config: " + item->name);
      item->latencyInt = -1;
      continue;
    }
    auto res = BuildConfig(item, true, false, ++index);
    if (!res->error.isEmpty()) {
      results->error = res->error;
      return results;
    }
    if (bean != nullptr && item->type == "custom" &&
        item->CustomBean()->core == "internal-full") {
      res->coreConfig["inbounds"] = QJsonArray();
      auto dns = res->coreConfig["dns"].toObject();
      dns.remove("fakeip");
      auto dnsLocalObj = BuildDnsObject("local", false);
      dnsLocalObj["tag"] = "dns-local";
      QJsonObject simpleDns;
      simpleDns["servers"] = QJsonArray{dnsLocalObj};
      res->coreConfig["dns"] = simpleDns;
      auto outbounds = res->coreConfig["outbounds"].toArray();
      QJsonArray testOutbounds;
      QString testOutboundTag;
      for (auto o : outbounds) {
        auto ob = o.toObject();
        ob.remove("domain_resolver");
        auto type = ob["type"].toString();
        auto tag = ob["tag"].toString();
        if (testOutboundTag.isEmpty() && type != "direct" && type != "block" &&
            type != "dns") {
          if (tag.isEmpty()) {
            tag = "proxy";
            ob["tag"] = tag;
          }
          testOutboundTag = tag;
        }
        if (type != "direct" && type != "block" && type != "dns") {
          ResolveOutboundServerForTestConfig(ob);
        }
        testOutbounds.append(ob);
      }
      res->coreConfig["outbounds"] = testOutbounds;
      auto route = res->coreConfig["route"].toObject();
      route.remove("rules");
      if (!testOutboundTag.isEmpty())
        route["final"] = testOutboundTag;
      QJsonObject defResolver;
      defResolver["server"] = "dns-local";
      route["default_domain_resolver"] = defResolver;
      res->coreConfig["route"] = route;
      results->fullConfigs[item->id] =
          QJsonObject2QString(res->coreConfig, true);
      continue;
    }

    // not full config, process it
    if (results->coreConfig.isEmpty()) {
      results->coreConfig = res->coreConfig;
    }
    // add the direct dns domains
    for (const auto &rule :
         res->coreConfig["dns"].toObject()["rules"].toArray()) {
      if (rule.toObject().contains("domain")) {
        for (const auto &domain : rule.toObject()["domain"].toArray()) {
          directDomainArray.append(domain);
        }
      }
    }
    // now we add the outbounds of the current config to the final one
    auto outbounds = res->coreConfig["outbounds"].toArray();
    if (outbounds.isEmpty()) {
      results->error = QString("outbounds is empty for %1").arg(item->name);
      return results;
    }
    auto endpoints = res->coreConfig["endpoints"].toArray();
    for (auto endpoint : endpoints)
      outbounds.append(endpoint);
    for (const auto &outboundRef : outbounds) {
      auto outbound = outboundRef.toObject();
      QString outboundTag = outbound["tag"].toString();

      if (outboundTag == "direct" || outboundTag == "block" ||
          outboundTag == "dns-out" ||
          outboundTag.startsWith("rout"))
        continue;
      if (outboundTag == "proxy") {
        if (index > 1)
          outboundTag += QString::number(index);
        outbound.insert("tag", outboundTag);
        if (
            QString outboundType = outbound.value("type").toString();
            outboundType == "wireguard" ||
            outboundType == "tailscale" ||
            outboundType == "awg"
        ) {
          endpointArray.append(outbound);
        } else {
          outboundArray.append(outbound);
        }
        results->outboundTags << outboundTag;
        results->tag2entID.insert(outboundTag, item->id);
        continue;
      }
      outboundArray.append(outbound);
    }
  }

  results->coreConfig["outbounds"] = outboundArray;
  results->coreConfig["endpoints"] = endpointArray;
  auto dnsObj = results->coreConfig["dns"].toObject();
  auto dnsRulesObj = QJsonArray();
  if (!directDomainArray.empty()) {
    dnsRulesObj += QJsonObject{{"domain", directDomainArray},
                               {"action", "route"},
                               {"server", "dns-direct"}};
  }
  dnsObj["rules"] = dnsRulesObj;
  results->coreConfig["dns"] = dnsObj;
  results->coreConfig["route"] = QJsonObject{
      {"auto_detect_interface", true},
      {"default_domain_resolver",
       QJsonObject{
           {"server", "dns-direct"},
           {"strategy", dataStore->routing->outbound_domain_strategy},
       }}};

  return results;
}

QList<std::shared_ptr<ProxyEntity>>
ResolveChainInternal(const std::shared_ptr<BuildConfigStatus> &status,
                     const std::shared_ptr<ProxyEntity> &ent) {
  QList<std::shared_ptr<ProxyEntity>> resolved;
  if (ent == nullptr) {
    return resolved;
  }
  auto bean = ent->bean();
  if (bean == nullptr) {
    return resolved;
  }
  if (ent->type == "chain") {
    auto list = ent->ChainBean()->list;
    std::reverse(std::begin(list), std::end(list));
    for (auto id : list) {
      auto profile = profileManager->GetProfile(id);
      ;
      if (profile == nullptr) {
        continue;
      }
      resolved += profile;
      if (resolved.last() == nullptr) {
        status->result->error = QString("chain missing ent: %1").arg(id);
        break;
      }
      if (resolved.last()->type == "chain") {
        status->result->error =
            QString("chain in chain is not allowed: %1").arg(id);
        break;
      }
    }
  } else {
    resolved += ent;
  };
  return resolved;
}

#define resolveChain(X) ResolveChainInternal(status, X);

QString BuildChain(int chainId,
                   const std::shared_ptr<BuildConfigStatus> &status) {
  auto group = profileManager->GetGroup(status->ent->gid);
  if (group == nullptr) {
    status->result->error = QString(
        "This profile is not in any group, your data may be corrupted.");
    return {};
  }

  QList<std::shared_ptr<ProxyEntity>> ents;
  if (group->landing_proxy_id >= 0) {
    auto lEnt = profileManager->GetProfile(group->landing_proxy_id);
    if (lEnt == nullptr) {
      status->result->error = QString("landing proxy ent not found.");
      return {};
    }
    ents = resolveChain(lEnt);
    if (!status->result->error.isEmpty())
      return {};
  }

  // Make list
  ents += resolveChain(status->ent);
  if (!status->result->error.isEmpty())
    return {};

  if (group->front_proxy_id >= 0) {
    auto fEnt = profileManager->GetProfile(group->front_proxy_id);
    if (fEnt == nullptr) {
      status->result->error = QString("front proxy ent not found.");
      return {};
    }
    ents += resolveChain(fEnt);
    if (!status->result->error.isEmpty())
      return {};
  }

  // BuildChain
  QString chainTagOut = BuildChainInternal(chainId, ents, status);

  // Chain ent traffic stat
  if (ents.length() > 1) {
    status->ent->traffic_data->id = status->ent->id;
    status->ent->traffic_data->tag = chainTagOut.toStdString();
    status->ent->traffic_data->ignoreForRate = true;
    status->result->outboundStats += status->ent->traffic_data;
  }

  return chainTagOut;
}

QString BuildChainInternal(int chainId,
                           const QList<std::shared_ptr<ProxyEntity>> &ents,
                           const std::shared_ptr<BuildConfigStatus> &status,
                           int route_suffix) {
  bool is_route = route_suffix >= 0;

  QString chainTag = "c-" + QString::number(chainId);
  QString chainTagOut = "";
  bool lastWasEndpoint = false;
  if (is_route) {
    chainTag = "r-" + QString::number(route_suffix) + "-" + chainTag;
  }

  for (int index = 0; index < ents.length(); index++) {
    auto ent = ents.at(index);
    if (ent == nullptr) {
      continue;
    }
    auto bean = ent->bean();
    if (bean == nullptr) {
      continue;
    }
    QString tagOut;

    // last profile set as "proxy"
    if (index == 0) {
      tagOut = "proxy";
      if (is_route) {
        tagOut = chainTag + "-" + tagOut;
      }
    } else {
      tagOut = chainTag + "-" + QString::number(ent->id) + "-" +
               QString::number(index);
    }

    if (index > 0) {
      // chain rules: past
      auto replaced = (lastWasEndpoint ? status->endpoints : status->outbounds)
                          .last()
                          .toObject();
      replaced["detour"] = tagOut;
      ent->traffic_data->isChainTail = true;
      (lastWasEndpoint ? status->endpoints : status->outbounds).removeLast();
      (lastWasEndpoint ? status->endpoints : status->outbounds) += replaced;
    } else {
      // index == 0 means last profile in chain / not chain
      chainTagOut = tagOut;
    }

    // Outbound

    QJsonObject outbound;

    BuildOutbound(ent, status, outbound, tagOut);
    // apply custom outbound settings
    MergeJson(QString2QJsonObject(bean->custom_outbound), outbound);

    // Bypass Lookup for the first profile
    auto serverAddress = ent->serverAddress;

    if (ent->type == "custom") {
      if (auto customBean = ent->CustomBean();
          customBean != nullptr && customBean->core == "internal") {
        auto server =
            QString2QJsonObject(customBean->config_simple)["server"].toString();
        if (!server.isEmpty())
          serverAddress = server;
      }
    }

    if (!IsIpAddress(serverAddress)) {
      status->domainListDNSDirect += serverAddress;
    }

    if (bean->IsEndpoint()) {
      status->endpoints += outbound;
      lastWasEndpoint = true;
    } else {
      status->outbounds += outbound;
      lastWasEndpoint = false;
    }
  }

  return chainTagOut;
}

static void AppendMissingRuleSet(QJsonObject &route, const QJsonObject &ruleSet) {
  if (ruleSet.isEmpty()) {
    return;
  }
  auto ruleSets = route["rule_set"].toArray();
  const auto tag = ruleSet["tag"].toString();
  for (const auto &existing : ruleSets) {
    if (!tag.isEmpty() && existing.toObject()["tag"].toString() == tag) {
      return;
    }
  }
  ruleSets += ruleSet;
  route["rule_set"] = ruleSets;
}

static QJsonArray BuildNekoboxTunRulesForFullConfig(
    const std::shared_ptr<BuildConfigStatus> &status, QJsonObject &config) {
  auto routeChain =
      profileManager->GetRouteChain(dataStore->routing->current_route_id);
  if (routeChain == nullptr) {
    return {};
  }

  std::map<int, QString> outboundMap;
  outboundMap[proxyID] = "proxy";
  outboundMap[directID] = "direct";
  outboundMap[blockID] = "block";

  for (const auto &item : *routeChain->get_used_outbounds()) {
    if (item < 0) {
      continue;
    }
    auto neededEnt = profileManager->GetProfile(item);
    if (neededEnt == nullptr) {
      status->result->error =
          "The routing profile is referencing outbounds that no longer exists, "
          "consider revising your settings";
      return {};
    }

    auto ents = resolveChain(neededEnt);
    if (!status->result->error.isEmpty()) {
      return {};
    }
    auto tag =
        BuildChainInternal(status->chainID, ents, status, status->routeID);
    status->routeID++;
    outboundMap[item] = tag;
  }

  auto extraOutbounds = config["outbounds"].toArray();
  for (const auto &outbound : status->outbounds) {
    extraOutbounds += outbound;
  }
  config["outbounds"] = extraOutbounds;

  auto extraEndpoints = config["endpoints"].toArray();
  for (const auto &endpoint : status->endpoints) {
    extraEndpoints += endpoint;
  }
  if (!extraEndpoints.isEmpty()) {
    config["endpoints"] = extraEndpoints;
  }

  auto route = config["route"].toObject();
  for (const auto &ruleSet : *routeChain->get_used_rule_sets()) {
    AppendMissingRuleSet(route, get_rule_set_json(ruleSet));
  }
  if (dataStore->adblock_enable) {
    AppendMissingRuleSet(route, get_rule_set_json("nekobox-adblocksingbox"));
  }
  config["route"] = route;

  auto routeRules = routeChain->get_route_rules(false, false, outboundMap);
  auto split = dataStore->routing->tun_split;
  if (!split->proxy.isEmpty()) {
    routeRules += QJsonObject{{"action", "route"},
                              {"outbound", "proxy"},
                              {"process_path", QListStr2QJsonArray(split->proxy)}};
  }
  if (!split->direct.isEmpty()) {
    routeRules += QJsonObject{
        {"action", "route"},
        {"outbound", "direct"},
        {"process_path", QListStr2QJsonArray(split->direct)}};
  }
  if (!split->block.isEmpty()) {
    routeRules += QJsonObject{{"action", "reject"},
                              {"process_path", QListStr2QJsonArray(split->block)}};
  }
  return routeRules;
}

void BuildOutbound(const std::shared_ptr<ProxyEntity> &ent,
                   const std::shared_ptr<BuildConfigStatus> &status,
                   QJsonObject &outbound, const QString &tag) {
  if (ent == nullptr) {
    return;
  }
  auto bean = ent->bean();
  if (bean == nullptr) {
    return;
  }
  if (ent->type == "wireguard" || ent->type == "awg") {
    if (ent->WireguardBean()->useSystemInterface && !IsAdmin()) {
      MW_dialog_message("configBuilder", "NeedAdmin");
      status->result->error =
          "using wireguard system interface requires elevated permissions";
      return;
    }
  }

  const auto coreR = bean->BuildCoreObjSingBox();
  if (coreR.outbound.isEmpty()) {
    status->result->error = "unsupported outbound";
    return;
  }
  if (!coreR.error.isEmpty()) { // rejected
    status->result->error = coreR.error;
    return;
  }
  outbound = coreR.outbound;

  // outbound misc
  outbound["tag"] = tag;
  ent->traffic_data->id = ent->id;
  ent->traffic_data->tag = tag.toStdString();
  status->result->outboundStats += ent->traffic_data;

  // mux common
  auto needMux = ent->type == "vmess" || ent->type == "trojan" ||
                 ent->type == "vless" || ent->type == "shadowsocks";
  needMux &= dataStore->mux_concurrency > 0;

  auto stream = GetStreamSettingsConst(bean.get());
  if (stream != nullptr) {
    QString network = *stream->network;
    if (network == "grpc" || network == "quic" || network == "anytls" ||
        (network == "http" && stream->security == "tls")) {
      needMux = false;
    }
  }

  auto mux_state = bean->mux_state;
  if (mux_state == 0) {
    if (!dataStore->mux_default_on && !bean->enable_brutal)
      needMux = false;
  } else if (mux_state == 1) {
    needMux = true;
  } else if (mux_state == 2) {
    needMux = false;
  }

  if (ent->type == "vless" && outbound["flow"] != "") {
    needMux = false;
  }

  // common
  // apply mux
  if (needMux) {
    auto muxObj = QJsonObject{
        {"enabled", true},
        {"protocol", dataStore->mux_protocol},
        {"padding", dataStore->mux_padding},
        {"max_streams", dataStore->mux_concurrency},
    };
    if (bean->enable_brutal) {
      auto brutalObj = QJsonObject{
          {"enabled", true},
          {"up_mbps", bean->brutal_speed},
          {"down_mbps", bean->brutal_speed},
      };
      muxObj["max_connections"] = 1;
      muxObj["brutal"] = brutalObj;
    }
    outbound["multiplex"] = muxObj;
  }
}

// SingBox

QJsonObject BuildDnsObject(QString address, bool tunEnabled) {
  //        bool usingSystemdResolved = false;
  // #ifdef Q_OS_LINUX
  //        usingSystemdResolved =
  //        ReadFileText("/etc/resolv.conf").contains("systemd-resolved");
  // #endif
  if (address.startsWith("local")) {
    //           if (tunEnabled && usingSystemdResolved)
    //           {
    //               return {
    //                   {"type", "underlying"}
    //               };
    //           }
    return {{"type", "local"}};
  }
  if (address.startsWith("dhcp://")) {
    auto ifcName = address.replace("dhcp://", "");
    if (ifcName == "auto")
      ifcName = "";
    return {
        {"type", "dhcp"},
        {"interface", ifcName},
    };
  }
  QString addr = address;
  int port = -1;
  QString type = "udp";
  QString path = "";
  if (address.startsWith("tcp://")) {
    type = "tcp";
    addr = addr.replace("tcp://", "");
  } else if (address.startsWith("tls://")) {
    type = "tls";
    addr = addr.replace("tls://", "");
  } else if (address.startsWith("udp://")) {
    type = "udp";
    addr = addr.replace("udp://", "");
  } else if (address.startsWith("quic://")) {
    type = "quic";
    addr = addr.replace("quic://", "");
  } else if (address.startsWith("https://")) {
    type = "https";
    addr = addr.replace("https://", "");
    if (addr.contains("/")) {
      path = addr.split("/").last();
      addr = addr.left(addr.indexOf("/"));
    }
  } else if (address.startsWith("h3://")) {
    type = "h3";
    addr = addr.replace("h3://", "");
    if (addr.contains("/")) {
      path = addr.split("/").last();
      addr = addr.left(addr.indexOf("/"));
    }
  }
  int colon_index = addr.lastIndexOf(u';');

  if (colon_index >= 0 && (colon_index + 1) < addr.size())
  {
    bool ok = false;
    int parsed_port = addr.sliced(colon_index + 1).toInt(&ok);  
    if (ok)
    {
      port = parsed_port;
      addr.truncate(colon_index); 
    }
  }
  QJsonObject res = {
      {"type", type},
      {"server", addr},
  };
  if (port != -1)
    res["server_port"] = port;
  if (!path.isEmpty())
    res["path"] = path;
  return res;
}

QJsonObject BuildTunInbound(const QStringList &directIPSets,
                            const QStringList &directIPCIDRs) {
  QJsonObject inboundObj;
  inboundObj["tag"] = "tun-in";
  inboundObj["type"] = "tun";
  inboundObj["interface_name"] = getTunName();
  inboundObj["auto_route"] = true;
  inboundObj["mtu"] = dataStore->vpn_mtu;
  inboundObj["stack"] = dataStore->vpn_implementation;
  inboundObj["strict_route"] = dataStore->vpn_strict_route;

  //         if (dataStore->auto_redirect){
  //                inboundObj["auto_redirect"] = true;
  //         }
  // #ifdef Q_OS_UNIX
  //             inboundObj["auto_redirect"] = true;
  // #endif
  auto tunAddress = QJsonArray{getTunAddress()};
  if (dataStore->vpn_ipv6)
    tunAddress += getTunAddress6();
  inboundObj["address"] = tunAddress;

  QJsonArray routeExcludeAddrs =
      QListStr2QJsonArray(Configs::dataStore->route_exclude_addrs);
  QJsonArray routeExcludeSets;
  if (dataStore->enable_tun_routing) {
    for (auto item : directIPCIDRs)
      routeExcludeAddrs << item;
    for (auto item : directIPSets)
      routeExcludeSets << item;
  }
  //    if (routeChain->defaultOutboundID != proxyID)
  {
    inboundObj["route_exclude_address"] = routeExcludeAddrs;
    if (!routeExcludeSets.isEmpty())
      inboundObj["route_exclude_address_set"] = routeExcludeSets;
  }
  return inboundObj;
}

void BuildConfigSingBox(const std::shared_ptr<BuildConfigStatus> &status) {
  bool blockAll = (status->ent == nullptr);
  // Prefetch
  std::shared_ptr<RoutingChain> routeChain;
  if (!blockAll) {
    routeChain =
        profileManager->GetRouteChain(dataStore->routing->current_route_id);
  } else {
    routeChain = RoutingChain::GetDefaultChain();
    routeChain->defaultOutboundID = blockID;
  }

  if (routeChain == nullptr) {
    status->result->error = "Routing profile does not exist, try resetting the "
                            "route profile in Routing Settings";
    return;
  }

  /*
  for (auto ruleItem: routeChain->Rules) {
      for (auto ruleSetItem = (ruleItem)->rule_set.begin(); ruleSetItem !=
  (ruleItem)->rule_set.end(); ++ruleSetItem) { if
  ((*ruleSetItem).endsWith("_IP")) { *ruleSetItem = "geoip-" +
  ruleSetItem->left(ruleSetItem->length() - 3); } else if
  ((*ruleSetItem).endsWith("_SITE")) { *ruleSetItem = "geosite-" +
  ruleSetItem->left(ruleSetItem->length() - 5);
          }
      }
  }
  routeChain->Save();

  if (dataStore->core_box_underlying_dns.isEmpty() && dataStore->spmode_vpn)
  {
      status->result->error = QObject::tr("Local DNS and Tun mode do not work
  together, please set an IP to be used as the Local DNS server in the Routing
  Settings -> Local override"); return;
  }
  */

  // copy for modification
  routeChain = std::make_shared<RoutingChain>(*routeChain);

  // Direct domains
  bool needDirectDnsRules = false;
  QJsonArray directDomains;
  QJsonArray directRuleSets;
  QJsonArray directSuffixes;
  QJsonArray directKeywords;
  QJsonArray directRegexes;
  // Direct IPs
  QStringList directIPSets;
  QStringList directIPCIDRs;
  QStringList sets;
  QStringList directIPraw;
  QString tagProxy;

  if (blockAll) {
    tagProxy = "block";
    goto skip_multiple_jobs;
  }
  // Outbounds
  tagProxy = BuildChain(status->chainID, status);
  if (!status->result->error.isEmpty())
    return;
  if (status->ent == nullptr) {
    status->result->error = "NullPointer ProxyEntity";
    return;
  }
  if (status->ent->type == "extracore") {
    auto bean = status->ent->ExtraCoreBean();
    if (bean == nullptr) {
      status->result->error = "Bean is null";
      return;
    }
    status->result->extraCoreData->path =
        QFileInfo(bean->extraCorePath).canonicalFilePath();
    status->result->extraCoreData->args = bean->extraCoreArgs;
    status->result->extraCoreData->config = bean->extraCoreConf;
    status->result->extraCoreData->configDir = GetBasePath();
    status->result->extraCoreData->noLog = bean->noLogs;
    routeChain->Rules << RouteRule::get_processPath_direct_rule(
        status->result->extraCoreData->path);
  }

  // server addresses
  for (const auto &item : status->domainListDNSDirect) {
    directDomains.append(item);
    needDirectDnsRules = true;
  }

  sets = routeChain->get_direct_sites();
  for (const auto &item : sets) {
    if (item.startsWith("ruleset:")) {
      directRuleSets << item.mid(8);
    }
    if (item.startsWith("domain:")) {
      directDomains << item.mid(7);
    }
    if (item.startsWith("suffix:")) {
      directSuffixes << item.mid(7);
    }
    if (item.startsWith("keyword:")) {
      directKeywords << item.mid(8);
    }
    if (item.startsWith("regex:")) {
      directRegexes << item.mid(6);
    }
    needDirectDnsRules = true;
  }

  directIPraw = routeChain->get_direct_ips();
  for (const auto &item : directIPraw) {
    if (item.startsWith("ruleset:")) {
      directIPSets << item.mid(8);
    }
    if (item.startsWith("ip:")) {
      directIPCIDRs << item.mid(3);
    }
  }

skip_multiple_jobs:
  // Inbounds
  // mixed-in
  int proxy_type = Configs::dataStore->inbound_proxy_type->value;
  if (IsValidPort(dataStore->inbound_socks_port) &&
      (!status->forTest || blockAll)
#ifndef USE_CPP_PROXY_CONFIGURATOR
      && Configs::dataStore->proxyInboundEnabled()
#endif
  ) {
    QJsonObject inboundObj;
    inboundObj["tag"] = "mixed-in";
    inboundObj["type"] =
#ifdef USE_CPP_PROXY_CONFIGURATOR
        "mixed"
#else
        (QString)*Configs::dataStore->inbound_proxy_type;
#endif
        inboundObj["listen"] = dataStore->inbound_address;
    inboundObj["listen_port"] = dataStore->inbound_socks_port;
    QString &inbound_username = dataStore->inbound_username;
    QString &inbound_password = dataStore->inbound_password;
    if (inbound_username != "" && inbound_password != "") {
      QJsonArray users;
      QJsonObject user;
      user["username"] = inbound_username;
      user["password"] = inbound_password;
      users += user;
      inboundObj["users"] = users;
    }
#ifndef USE_CPP_PROXY_CONFIGURATOR
    inboundObj["set_system_proxy"] = Configs::dataStore->spmode_system_proxy;
#endif
    status->inbounds += inboundObj;
  }

  // tun-in
  if ((dataStore->spmode_vpn && !status->forTest) || blockAll) {
    status->inbounds += BuildTunInbound(directIPSets, directIPCIDRs);
  }

  // ntp
  if (dataStore->enable_ntp && !blockAll) {
    QJsonObject ntpObj;
    ntpObj["enabled"] = true;
    ntpObj["server"] = dataStore->ntp_server_address;
    ntpObj["server_port"] = dataStore->ntp_server_port;
    ntpObj["interval"] = dataStore->ntp_interval;
    status->result->coreConfig["ntp"] = ntpObj;
  }

  // direct
  status->outbounds += QJsonObject{
      {"type", "direct"},
      {"tag", "direct"},
  };
  status->outbounds += QJsonObject{
      {"type", "block"},
      {"tag", "block"},
  };
  status->result->outboundStats +=
      std::make_shared<Stats::TrafficData>("direct");

  // Hijack
  if (dataStore->enable_dns_server && !status->forTest) {
    auto sniffRule = std::make_shared<RouteRule>();
    sniffRule->action = "sniff";
    sniffRule->inbound = {"dns-in"};

    auto redirRule = std::make_shared<RouteRule>();
    redirRule->action = "hijack-dns";
    redirRule->inbound = {"dns-in"};

    routeChain->Rules.prepend(redirRule);
    routeChain->Rules.prepend(sniffRule);
  }
  if (dataStore->enable_redirect && !status->forTest) {
    status->inbounds.prepend(QJsonObject{
        {"tag", "hijack"},
        {"type", "direct"},
        {"listen", dataStore->redirect_listen_address},
        {"listen_port", dataStore->redirect_listen_port},
    });
    auto sniffRule = std::make_shared<RouteRule>();
    sniffRule->action = "sniff";
    sniffRule->sniffOverrideDest = true;
    sniffRule->inbound = {"hijack"};
    routeChain->Rules.prepend(sniffRule);
  }

  // custom inbound
  if (!status->forTest)
    QJSONARRAY_ADD(
        status->inbounds,
        QString2QJsonObject(dataStore->custom_inbound)["inbounds"].toArray())

  // DNS hijack deps
  QJsonArray hijackDomains;
  QJsonArray hijackDomainSuffix;
  QJsonArray hijackDomainRegex;
  QJsonArray hijackGeoAssets;

  // manage routing section
  auto routeObj = QJsonObject();
  if (dataStore->spmode_vpn || blockAll) {
    routeObj["auto_detect_interface"] = true;
  }
  if (!status->forTest) {
    if (dataStore->connection_statistics) {
      routeObj["find_process"] = true;
    }
    routeObj["final"] = outboundIDToString(routeChain->defaultOutboundID);

    if (!dataStore->routing->domain_strategy.isEmpty()) {
      auto resolveRule = std::make_shared<RouteRule>();
      resolveRule->action = "resolve";
      resolveRule->strategy = dataStore->routing->domain_strategy;
      resolveRule->inbound = {"mixed-in", "tun-in"};
      routeChain->Rules.prepend(resolveRule);
    }
    if (dataStore->routing->sniffing_mode != SniffingMode::DISABLE) {
      auto sniffRule = std::make_shared<RouteRule>();
      sniffRule->action = "sniff";
      sniffRule->inbound = {"mixed-in", "tun-in"};
      routeChain->Rules.prepend(sniffRule);
    }
    auto neededOutbounds = routeChain->get_used_outbounds();
    auto neededRuleSets = routeChain->get_used_rule_sets();
    std::map<int, QString> outboundMap;
    outboundMap[proxyID] = "proxy";
    outboundMap[directID] = "direct";
    outboundMap[blockID] = "block";
    int suffix = 0;
    for (const auto &item : *neededOutbounds) {
      if (item < 0)
        continue;
      auto neededEnt = profileManager->GetProfile(item);
      if (neededEnt == nullptr) {
        status->result->error =
            "The routing profile is referencing outbounds that no longer "
            "exists, consider revising your settings";
        return;
      }

      auto ents = resolveChain(neededEnt);
      if (!status->result->error.isEmpty())
        return;
      auto tag =
          BuildChainInternal(status->chainID, ents, status, status->routeID);
      status->routeID++;

      outboundMap[item] = tag;

      // add to dns direct resolve
      if (!IsIpAddress(neededEnt->serverAddress)) {
        directDomains << neededEnt->serverAddress;
        needDirectDnsRules = true;
      }
    }

    auto routeRules = routeChain->get_route_rules(false, false, outboundMap);

    // tun process routing
    if (dataStore->spmode_vpn && !status->forTest) {
      auto split = dataStore->routing->tun_split;
      if (split->proxy.size() > 0) {
        routeRules +=
            QJsonObject({{"action", "route"},
                         {"outbound", "proxy"},
                         {"process_path", QListStr2QJsonArray(split->proxy)}});
      }

      if (split->direct.size() > 0) {
        routeRules +=
            QJsonObject({{"action", "route"},
                         {"outbound", "direct"},
                         {"process_path", QListStr2QJsonArray(split->direct)}});
      }

      if (split->block.size() > 0) {
        routeRules +=
            QJsonObject({{"action", "reject"},
                         {"process_path", QListStr2QJsonArray(split->block)}});
      }
    }

    routeObj["rules"] = routeRules;

    if (dataStore->enable_dns_server) {
      for (const auto &rule : dataStore->dns_server_rules) {
        if (rule.startsWith("ruleset:")) {
          hijackGeoAssets << rule.mid(8);
        }
        if (rule.startsWith("domain:")) {
          hijackDomains << rule.mid(7);
        }
        if (rule.startsWith("suffix:")) {
          hijackDomainSuffix << rule.mid(7);
        }
        if (rule.startsWith("regex:")) {
          hijackDomainRegex << rule.mid(6);
        }
      }
    }
    for (auto ruleSet : hijackGeoAssets) {
      if (!neededRuleSets->contains(ruleSet.toString()))
        neededRuleSets->append(ruleSet.toString());
    }

    auto ruleSetArray = QJsonArray();
    for (const auto &item : *neededRuleSets) {
      auto json_object = get_rule_set_json(item);
      if (!json_object.isEmpty()) {
        ruleSetArray += json_object;
      }
    }
    if (dataStore->adblock_enable && !blockAll) {
      QString item = "nekobox-adblocksingbox";
      auto json_object = get_rule_set_json(item);
      if (!json_object.isEmpty()) {
        ruleSetArray += json_object;
      }
    }
    routeObj["rule_set"] = ruleSetArray;
  }

  // DNS settings
  QJsonObject dns;
  QJsonArray dnsServers;
  QJsonArray dnsRules;

  routeObj["default_domain_resolver"] = QJsonObject{
      {"server", "dns-direct"},
      {"strategy", dataStore->routing->outbound_domain_strategy},
  };

  // Remote
  if (blockAll) {

  } else if (status->ent->type == "tailscale") {
    auto tailDns = QJsonObject{
        {"type", "tailscale"},
        {"tag", "dns-remote"},
        {"endpoint", "proxy"},
        {"accept_default_resolvers", status->ent->TailscaleBean()->globalDNS},
    };
    dnsServers += tailDns;
  } else {
    auto remoteDnsObj =
        BuildDnsObject(dataStore->routing->remote_dns, dataStore->spmode_vpn);
    remoteDnsObj["tag"] = "dns-remote";
    remoteDnsObj["domain_resolver"] = "dns-local";
    remoteDnsObj["detour"] = tagProxy;
    dnsServers += remoteDnsObj;
  }

  // Direct
  auto directDNSAddress = dataStore->routing->direct_dns;
  auto directDnsObj = BuildDnsObject(directDNSAddress, dataStore->spmode_vpn);
  directDnsObj["tag"] = "dns-direct";
  directDnsObj["domain_resolver"] = "dns-local";

  // default dns server
  if (dataStore->routing->dns_final_out_direct) {
    dnsServers.prepend(directDnsObj);
  } else {
    dnsServers.append(directDnsObj);
  }

  // Handle localhost
  dnsRules += QJsonObject{
      {"domain", "localhost"},
      {"action", "predefined"},
      {"query_type", "A"},
      {"rcode", "NOERROR"},
      {"answer", "localhost. IN A 127.0.0.1"},
  };

  dnsRules += QJsonObject{
      {"domain", "localhost"},
      {"action", "predefined"},
      {"query_type", "AAAA"},
      {"rcode", "NOERROR"},
      {"answer", "localhost. IN AAAA ::1"},
  };

  // Hijack
  if (dataStore->enable_dns_server && !status->forTest && !blockAll) {
    dnsRules += QJsonObject{
        {"rule_set", hijackGeoAssets},
        {"domain", hijackDomains},
        {"domain_suffix", hijackDomainSuffix},
        {"domain_regex", hijackDomainRegex},
        {"query_type", "A"},
        {"action", "predefined"},
        {"rcode", "NOERROR"},
        {"answer", QString("* IN A %1").arg(dataStore->dns_v4_resp)},
    };

    if (!dataStore->dns_v6_resp.isEmpty()) {
      dnsRules += QJsonObject{
          {"rule_set", hijackGeoAssets},
          {"domain", hijackDomains},
          {"domain_suffix", hijackDomainSuffix},
          {"domain_regex", hijackDomainRegex},
          {"query_type", "AAAA"},
          {"action", "predefined"},
          {"rcode", "NOERROR"},
          {"answer", QString("* IN AAAA %1").arg(dataStore->dns_v6_resp)},
      };
    }

    status->inbounds.prepend(QJsonObject{
        {"tag", "dns-in"},
        {"type", "direct"},
        {"listen", dataStore->dns_server_listen_lan ? "0.0.0.0" : "127.1.1.1"},
        {"listen_port", dataStore->dns_server_listen_port},
    });
  }

  // Fakedns
  if (dataStore->fake_dns && !blockAll) {
    dnsServers += QJsonObject{
        {"tag", "dns-fake"},
        {"type", "fakeip"},
        {"inet4_range", "198.18.0.0/15"},
        {"inet6_range", "fc00::/18"},
    };
    dnsRules += QJsonObject{{"query_type", QJsonArray{"A", "AAAA"}},
                            {"action", "route"},
                            {"server", "dns-fake"}};
    dns["independent_cache"] = true;
  }

  if (needDirectDnsRules && !blockAll) {
    dnsRules += QJsonObject{
        {"rule_set", directRuleSets},      {"domain", directDomains},
        {"domain_suffix", directSuffixes}, {"domain_keyword", directKeywords},
        {"domain_regex", directRegexes},   {"action", "route"},
        {"server", "dns-direct"},
    };
  }

  // Underlying DNS
  auto dnsLocalAddress = dataStore->core_box_underlying_dns.isEmpty()
                             ? "local"
                             : dataStore->core_box_underlying_dns;
  auto dnsLocalObj = BuildDnsObject(dnsLocalAddress, dataStore->spmode_vpn);
  dnsLocalObj["tag"] = "dns-local";
  dnsServers += dnsLocalObj;

  dns["servers"] = dnsServers;
  dns["rules"] = dnsRules;

  if (!dataStore->routing->dns_final_out_direct) {
    dns["final"] = "dns-remote";
  }

  if (dataStore->routing->use_dns_object) {
    dns = QString2QJsonObject(dataStore->routing->dns_object);
  }

  // experimental
  QJsonObject experimentalObj;
  QJsonObject clash_api = {
      {"default_mode", ""} // dummy to make sure it is created
  };

  if (!status->forTest) {
    if (dataStore->core_box_clash_api > 0) {
      clash_api = {
          {"external_controller",
           dataStore->core_box_clash_listen_addr + ":" +
               QString::number(dataStore->core_box_clash_api)},
          {"secret", dataStore->core_box_clash_api_secret},
          {"external_ui", "dashboard"},
      };
    }
    if (dataStore->core_box_clash_api > 0 || dataStore->connection_statistics) {
      experimentalObj["clash_api"] = clash_api;
    }
  }

  if (!blockAll) {
    QString cache_id = serverName;
    if (status->forTest) {
      cache_id += "-test.db";
    } else {
      cache_id += "-core.db";
    }
    QJsonObject cache_file = {
        {"enabled", true},
        {"path", cache_id},
        {"store_fakeip", true},
        {"store_rdrc", true},
        //            {"path", cachePath + "/nekobox_cache.db"}
    };
    experimentalObj["cache_file"] = cache_file;
  }

  status->result->coreConfig.insert(
      "log", QJsonObject{{"level", dataStore->log_level}});
  status->result->coreConfig.insert(
      "certificate",
      QJsonObject{
          {"store", dataStore->use_mozilla_certs ? "mozilla" : "system"}});
  status->result->coreConfig.insert("dns", dns);
  status->result->coreConfig.insert("inbounds", status->inbounds);
  status->result->coreConfig.insert("outbounds", status->outbounds);
  status->result->coreConfig.insert("endpoints", status->endpoints);
  status->result->coreConfig.insert("route", routeObj);
  if (!experimentalObj.isEmpty())
    status->result->coreConfig.insert("experimental", experimentalObj);
}

QString get_jsdelivr_link(QString link) {
  if (dataStore->routing->ruleset_mirror == Mirrors::GITHUB)
    return link;
  if (auto url = QUrl(link);
      url.isValid() && url.host() == "raw.githubusercontent.com") {
    QStringList list = url.path().split('/');
    QString result;
    switch (dataStore->routing->ruleset_mirror) {
    case Mirrors::GCORE:
      result = "https://gcore.jsdelivr.net/gh";
      break;
    case Mirrors::QUANTIL:
      result = "https://quantil.jsdelivr.net/gh";
      break;
    case Mirrors::FASTLY:
      result = "https://fastly.jsdelivr.net/gh";
      break;
    case Mirrors::CDN:
      result = "https://cdn.jsdelivr.net/gh";
      break;
    default:
      result = "https://testingcf.jsdelivr.net/gh";
    }

    int index = 0;
    foreach (QString item, list) {
      if (!item.isEmpty()) {
        if (index == 2)
          result += "@" + item;
        else
          result += "/" + item;
        index++;
      }
    }
    return result;
  }
  return link;
}
} // namespace Configs
