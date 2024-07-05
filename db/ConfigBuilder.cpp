#include "db/ConfigBuilder.hpp"
#include "db/Database.hpp"
#include "fmt/includes.h"
#include "fmt/Preset.hpp"
#include "rpc/gRPC.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>

#define BOX_UNDERLYING_DNS_EXPORT dataStore->core_box_underlying_dns.isEmpty() ? (status->forExport ? "local" : "underlying://0.0.0.0") : dataStore->core_box_underlying_dns

namespace NekoGui {

    QStringList getAutoBypassExternalProcessPaths(const std::shared_ptr<BuildConfigResult> &result) {
        QStringList paths;
        for (const auto &extR: result->extRs) {
            auto path = extR->program;
            if (path.trimmed().isEmpty()) continue;
            paths << path.replace("\\", "/");
        }
        return paths;
    }

    QString genTunName() {
        auto tun_name = "nekoray-tun";
#ifdef Q_OS_MACOS
        tun_name = "utun9";
#endif
        return tun_name;
    }

    void MergeJson(const QJsonObject &custom, QJsonObject &outbound) {
        // 合并
        if (custom.isEmpty()) return;
        for (const auto &key: custom.keys()) {
            if (outbound.contains(key)) {
                auto v = custom[key];
                auto v_orig = outbound[key];
                if (v.isObject() && v_orig.isObject()) { // isObject 则合并？
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

    std::shared_ptr<BuildConfigResult> BuildConfig(const std::shared_ptr<ProxyEntity> &ent, bool forTest, bool forExport) {
        auto result = std::make_shared<BuildConfigResult>();
        auto status = std::make_shared<BuildConfigStatus>();
        status->ent = ent;
        status->result = result;
        status->forTest = forTest;
        status->forExport = forExport;

        auto customBean = dynamic_cast<NekoGui_fmt::CustomBean *>(ent->bean.get());
        if (customBean != nullptr && customBean->core == "internal-full") {
            result->coreConfig = QString2QJsonObject(customBean->config_simple);
        } else {
            BuildConfigSingBox(status);
        }

        // apply custom config
        MergeJson(QString2QJsonObject(ent->bean->custom_config), result->coreConfig);

        return result;
    }

    QString BuildChain(int chainId, const std::shared_ptr<BuildConfigStatus> &status) {
        auto group = profileManager->GetGroup(status->ent->gid);
        if (group == nullptr) {
            status->result->error = QString("This profile is not in any group, your data may be corrupted.");
            return {};
        }

        auto resolveChain = [=](const std::shared_ptr<ProxyEntity> &ent) {
            QList<std::shared_ptr<ProxyEntity>> resolved;
            if (ent->type == "chain") {
                auto list = ent->ChainBean()->list;
                std::reverse(std::begin(list), std::end(list));
                for (auto id: list) {
                    resolved += profileManager->GetProfile(id);
                    if (resolved.last() == nullptr) {
                        status->result->error = QString("chain missing ent: %1").arg(id);
                        break;
                    }
                    if (resolved.last()->type == "chain") {
                        status->result->error = QString("chain in chain is not allowed: %1").arg(id);
                        break;
                    }
                }
            } else {
                resolved += ent;
            };
            return resolved;
        };

        // Make list
        auto ents = resolveChain(status->ent);
        if (!status->result->error.isEmpty()) return {};

        if (group->front_proxy_id >= 0) {
            auto fEnt = profileManager->GetProfile(group->front_proxy_id);
            if (fEnt == nullptr) {
                status->result->error = QString("front proxy ent not found.");
                return {};
            }
            ents += resolveChain(fEnt);
            if (!status->result->error.isEmpty()) return {};
        }

        if (group->landing_proxy_id >= 0) {
            auto lEnt = profileManager->GetProfile(group->landing_proxy_id);
            if (lEnt == nullptr) {
                status->result->error = QString("landing proxy ent not found.");
                return {};
            }
            ents = resolveChain(lEnt) + ents;
            if (!status->result->error.isEmpty()) return {};
        }

        // BuildChain
        QString chainTagOut = BuildChainInternal(0, ents, status);

        // Chain ent traffic stat
        if (ents.length() > 1) {
            status->ent->traffic_data->id = status->ent->id;
            status->ent->traffic_data->tag = chainTagOut.toStdString();
            status->result->outboundStats += status->ent->traffic_data;
        }

        return chainTagOut;
    }

    QString BuildChainInternal(int chainId, const QList<std::shared_ptr<ProxyEntity>> &ents,
                               const std::shared_ptr<BuildConfigStatus> &status) {
        QString chainTag = "c-" + Int2String(chainId);
        QString chainTagOut;

        QString pastTag;
        int pastExternalStat = 0;
        int index = 0;

        for (const auto &ent: ents) {
            // tagOut: v2ray outbound tag for a profile
            // profile2 (in) (global)   tag g-(id)
            // profile1                 tag (chainTag)-(id)
            // profile0 (out)           tag (chainTag)-(id) / single: chainTag=g-(id)
            auto tagOut = chainTag + "-" + Int2String(ent->id);

            // needGlobal: can only contain one?
            bool needGlobal = false;

            // first profile set as global
            auto isFirstProfile = index == ents.length() - 1;
            if (isFirstProfile) {
                needGlobal = true;
                tagOut = "g-" + Int2String(ent->id);
            }

            // last profile set as "proxy"
            if (chainId == 0 && index == 0) {
                needGlobal = false;
                tagOut = "proxy";
            }

            // ignoreConnTag
            if (index != 0) {
                status->result->ignoreConnTag << tagOut;
            }

            if (needGlobal) {
                if (status->globalProfiles.contains(ent->id)) {
                    continue;
                }
                status->globalProfiles += ent->id;
            }

            if (index > 0) {
                // chain rules: past
                if (pastExternalStat == 0) {
                    auto replaced = status->outbounds.last().toObject();
                    replaced["detour"] = tagOut;
                    status->outbounds.removeLast();
                    status->outbounds += replaced;
                } else {
                    status->routingRules += QJsonObject{
                        {"inbound", QJsonArray{pastTag + "-mapping"}},
                        {"outbound", tagOut},
                    };
                }
            } else {
                // index == 0 means last profile in chain / not chain
                chainTagOut = tagOut;
                status->result->outboundStat = ent->traffic_data;
            }

            // chain rules: this
            auto ext_mapping_port = 0;
            auto ext_socks_port = 0;
            auto thisExternalStat = ent->bean->NeedExternal(isFirstProfile);
            if (thisExternalStat < 0) {
                status->result->error = "This configuration cannot be set automatically, please try another.";
                return {};
            }

            // determine port
            if (thisExternalStat > 0) {
                if (ent->type == "custom") {
                    auto bean = ent->CustomBean();
                    if (IsValidPort(bean->mapping_port)) {
                        ext_mapping_port = bean->mapping_port;
                    } else {
                        ext_mapping_port = MkPort();
                    }
                    if (IsValidPort(bean->socks_port)) {
                        ext_socks_port = bean->socks_port;
                    } else {
                        ext_socks_port = MkPort();
                    }
                } else {
                    ext_mapping_port = MkPort();
                    ext_socks_port = MkPort();
                }
            }
            if (thisExternalStat == 2) dataStore->need_keep_vpn_off = true;
            if (thisExternalStat == 1) {
                // mapping
                status->inbounds += QJsonObject{
                    {"type", "direct"},
                    {"tag", tagOut + "-mapping"},
                    {"listen", "127.0.0.1"},
                    {"listen_port", ext_mapping_port},
                    {"override_address", ent->bean->serverAddress},
                    {"override_port", ent->bean->serverPort},
                };
                // no chain rule and not outbound, so need to set to direct
                if (isFirstProfile) {
                    status->routingRules += QJsonObject{
                        {"inbound", QJsonArray{tagOut + "-mapping"}},
                        {"outbound", "direct"},
                    };
                }
            }

            // Outbound

            QJsonObject outbound;

            if (thisExternalStat > 0) {
                auto extR = ent->bean->BuildExternal(ext_mapping_port, ext_socks_port, thisExternalStat);
                if (extR.program.isEmpty()) {
                    status->result->error = QObject::tr("Core not found: %1").arg(ent->bean->DisplayCoreType());
                    return {};
                }
                if (!extR.error.isEmpty()) { // rejected
                    status->result->error = extR.error;
                    return {};
                }
                extR.tag = ent->bean->DisplayType();
                status->result->extRs.emplace_back(std::make_shared<NekoGui_fmt::ExternalBuildResult>(extR));

                // SOCKS OUTBOUND
                outbound["type"] = "socks";
                outbound["server"] = "127.0.0.1";
                outbound["server_port"] = ext_socks_port;
                // outbound misc
                outbound["tag"] = tagOut;
                ent->traffic_data->id = ent->id;
                ent->traffic_data->tag = tagOut.toStdString();
                status->result->outboundStats += ent->traffic_data;
            } else {
                BuildOutbound(ent, status, outbound, tagOut);
            }

            // apply custom outbound settings
            MergeJson(QString2QJsonObject(ent->bean->custom_outbound), outbound);

            // Bypass Lookup for the first profile
            auto serverAddress = ent->bean->serverAddress;

            auto customBean = dynamic_cast<NekoGui_fmt::CustomBean *>(ent->bean.get());
            if (customBean != nullptr && customBean->core == "internal") {
                auto server = QString2QJsonObject(customBean->config_simple)["server"].toString();
                if (!server.isEmpty()) serverAddress = server;
            }

            if (!IsIpAddress(serverAddress)) {
                status->domainListDNSDirect += serverAddress;
            }

            status->outbounds += outbound;
            pastTag = tagOut;
            pastExternalStat = thisExternalStat;
            index++;
        }

        return chainTagOut;
    }

    void BuildOutbound(const std::shared_ptr<ProxyEntity> &ent, const std::shared_ptr<BuildConfigStatus> &status, QJsonObject& outbound, const QString& tag) {
        const auto coreR = ent->bean->BuildCoreObjSingBox();
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
        auto needMux = ent->type == "vmess" || ent->type == "trojan" || ent->type == "vless" || ent->type == "shadowsocks";
        needMux &= dataStore->mux_concurrency > 0;

        auto stream = GetStreamSettings(ent->bean.get());
        if (stream != nullptr) {
            if (stream->network == "grpc" || stream->network == "quic" || (stream->network == "http" && stream->security == "tls")) {
                needMux = false;
            }
        }

        auto mux_state = ent->bean->mux_state;
        if (mux_state == 0) {
            if (!dataStore->mux_default_on && !ent->bean->enable_brutal) needMux = false;
        } else if (mux_state == 1) {
            needMux = true;
        } else if (mux_state == 2) {
            needMux = false;
        }

        if (ent->type == "vless" && outbound["flow"] != "") {
            needMux = false;
        }

        // common
        // apply domain_strategy
        outbound["domain_strategy"] = dataStore->routing->outbound_domain_strategy;
        // apply mux
        if (needMux) {
            auto muxObj = QJsonObject{
                {"enabled", true},
                {"protocol", dataStore->mux_protocol},
                {"padding", dataStore->mux_padding},
                {"max_streams", dataStore->mux_concurrency},
            };
            if (ent->bean->enable_brutal) {
                auto brutalObj = QJsonObject{
                    {"enabled", true},
                    {"up_mbps", ent->bean->brutal_speed},
                    {"down_mbps", ent->bean->brutal_speed},
                };
                muxObj["max_connections"] = 1;
                muxObj["brutal"] = brutalObj;
            }
            outbound["multiplex"] = muxObj;
        }
    }

    // SingBox

    void BuildConfigSingBox(const std::shared_ptr<BuildConfigStatus> &status) {
        // Inbounds

        // mixed-in
        if (IsValidPort(dataStore->inbound_socks_port) && !status->forTest) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "mixed-in";
            inboundObj["type"] = "mixed";
            inboundObj["listen"] = dataStore->inbound_address;
            inboundObj["listen_port"] = dataStore->inbound_socks_port;
            if (dataStore->routing->sniffing_mode != SniffingMode::DISABLE) {
                inboundObj["sniff"] = true;
                inboundObj["sniff_override_destination"] = dataStore->routing->sniffing_mode == SniffingMode::FOR_DESTINATION;
            }
            if (dataStore->inbound_auth->NeedAuth()) {
                inboundObj["users"] = QJsonArray{
                    QJsonObject{
                        {"username", dataStore->inbound_auth->username},
                        {"password", dataStore->inbound_auth->password},
                    },
                };
            }
            inboundObj["domain_strategy"] = dataStore->routing->domain_strategy;
            status->inbounds += inboundObj;
        }

        // tun-in
        if (dataStore->spmode_vpn && !status->forTest) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "tun-in";
            inboundObj["type"] = "tun";
            inboundObj["interface_name"] = genTunName();
            inboundObj["auto_route"] = true;
            inboundObj["endpoint_independent_nat"] = true;
            inboundObj["mtu"] = dataStore->vpn_mtu;
            inboundObj["stack"] = Preset::SingBox::VpnImplementation.value(dataStore->vpn_implementation);
            inboundObj["strict_route"] = dataStore->vpn_strict_route;
            inboundObj["gso"] = dataStore->enable_gso;
            inboundObj["inet4_address"] = "172.19.0.1/28";
            if (dataStore->vpn_ipv6) inboundObj["inet6_address"] = "fdfe:dcba:9876::1/126";
            if (dataStore->routing->sniffing_mode != SniffingMode::DISABLE) {
                inboundObj["sniff"] = true;
                inboundObj["sniff_override_destination"] = dataStore->routing->sniffing_mode == SniffingMode::FOR_DESTINATION;
            }
            inboundObj["domain_strategy"] = dataStore->routing->domain_strategy;
            status->inbounds += inboundObj;
        }

        // ntp
        if (dataStore->enable_ntp) {
            QJsonObject ntpObj;
            ntpObj["enabled"] = true;
            ntpObj["server"] = dataStore->ntp_server_address;
            ntpObj["server_port"] = dataStore->ntp_server_port;
            ntpObj["interval"] = dataStore->ntp_interval;
            status->result->coreConfig["ntp"] = ntpObj;
        }

        // Outbounds
        auto tagProxy = BuildChain(0, status);
        if (!status->result->error.isEmpty()) return;

        // direct & block & dns-out
        status->outbounds += QJsonObject{
            {"type", "direct"},
            {"tag", "direct"},
        };
        status->outbounds += QJsonObject{
            {"type", "block"},
            {"tag", "block"},
        };
        status->outbounds += QJsonObject{
            {"type", "dns"},
            {"tag", "dns-out"},
        };

        // custom inbound
        if (!status->forTest) QJSONARRAY_ADD(status->inbounds, QString2QJsonObject(dataStore->custom_inbound)["inbounds"].toArray())

        // Routing
        // geopath
        auto geoip = FindCoreAsset("geoip.db");
        auto geosite = FindCoreAsset("geosite.db");
        if (geoip.isEmpty()) status->result->error = +"geoip.db not found, it is needed for generating rule sets";
        if (geosite.isEmpty()) status->result->error = +"geosite.db not found, it is needed for generating rule sets";

        // manage routing section
        auto routeObj = QJsonObject {
            {"auto_detect_interface", true},
        };
        routeObj["final"] = dataStore->routing->def_outbound;

        auto routeChain = NekoGui::profileManager->GetRouteChain(NekoGui::dataStore->routing->current_route_id);
        if (routeChain == nullptr) {
            status->result->error = "Routing profile does not exist, try resetting the route profile in Routing Settings";
            return;
        }
        auto neededOutbounds = routeChain->get_used_outbounds();
        auto neededRuleSets = routeChain->get_used_rule_sets();
        std::map<int, QString> outboundMap;
        outboundMap[-1] = "proxy";
        outboundMap[-2] = "direct";
        outboundMap[-3] = "block";
        outboundMap[-4] = "dns-out";
        int suffix = 0;
        for (const auto &item: *neededOutbounds) {
            if (item < 0) continue;
            auto neededEnt = NekoGui::profileManager->GetProfile(item);
            if (neededEnt == nullptr) {
                status->result->error = "The routing profile is referencing outbounds that no longer exists, consider revising your settings";
                return;
            }
            QJsonObject currOutbound;
            QString tag = "rout-" + Int2String(suffix++);
            BuildOutbound(neededEnt, status, currOutbound, tag);
            status->outbounds += currOutbound;
            outboundMap[item] = tag;

            // add to dns direct resolve
            if (!IsIpAddress(neededEnt->bean->serverAddress)) {
                status->domainListDNSDirect << neededEnt->bean->serverAddress;
            }
        }
        routeObj["rules"] = routeChain->get_route_rules(false, outboundMap);

        auto ruleSetArray = QJsonArray();
        for (const auto &item: *neededRuleSets) {
            ruleSetArray += QJsonObject{
                {"type", "local"},
                {"tag", item},
                {"format", "binary"},
                {"path", RULE_SETS_DIR + QString("/%1.srs").arg(item)},
            };
            if (QFile(QString(RULE_SETS_DIR + "/%1.srs").arg(item)).exists()) continue;
            bool ok;
            auto err = NekoGui_rpc::defaultClient->CompileGeoSet(&ok, item.contains("_IP") ? NekoGui_rpc::GeoRuleSetType::ip : NekoGui_rpc::GeoRuleSetType::site, item.toStdString());
            if (!ok) {
                MW_show_log("Failed to generate rule set asset for " + item);
                status->result->error = err;
                return;
            }
        }
        routeObj["rule_set"] = ruleSetArray;

        // DNS settings
        // final add DNS
        QJsonObject dns;
        QJsonArray dnsServers;
        QJsonArray dnsRules;

        // Remote
        dnsServers += QJsonObject{
            {"tag", "dns-remote"},
            {"address_resolver", "dns-local"},
            {"strategy", dataStore->routing->remote_dns_strategy},
            {"address", dataStore->routing->remote_dns},
            {"detour", tagProxy},
        };

        // Direct
        auto directDNSAddress = dataStore->routing->direct_dns;
        if (directDNSAddress == "localhost") directDNSAddress = BOX_UNDERLYING_DNS_EXPORT;
        QJsonObject directObj{
            {"tag", "dns-direct"},
            {"address_resolver", "dns-local"},
            {"strategy", dataStore->routing->direct_dns_strategy},
            {"address", directDNSAddress.replace("+local://", "://")},
            {"detour", "direct"},
        };
        if (dataStore->routing->dns_final_out == "direct") {
            dnsServers.prepend(directObj);
        } else {
            dnsServers.append(directObj);
        }

        // block
        dnsServers += QJsonObject{
            {"tag", "dns-block"},
            {"address", "rcode://success"},
        };

        // Fakedns
        if (dataStore->fake_dns && dataStore->spmode_vpn && !status->forTest) {
            dnsServers += QJsonObject{
                {"tag", "dns-fake"},
                {"address", "fakeip"},
            };
            dns["fakeip"] = QJsonObject{
                {"enabled", true},
                {"inet4_range", "198.18.0.0/15"},
                {"inet6_range", "fc00::/18"},
            };
        }

        // Direct dns domains
        QJsonArray directDnsDomains;
        for (const auto &item: status->domainListDNSDirect) {
            directDnsDomains.append(item);
        }
        if (!directDnsDomains.isEmpty()) {
            dnsRules += QJsonObject{
                {"domain", directDnsDomains},
                {"server", "dns-direct"},
            };
        }

        // Direct domains in rules
        QJsonArray directDnsRuleSets;
        auto sets = routeChain->get_direct_site_rule_sets();
        for (const auto &item: sets) {
            directDnsRuleSets << item;
        }
        dnsRules += QJsonObject{
            {"rule_set", directDnsRuleSets},
            {"server", "dns-direct"},
        };

        // Underlying 100% Working DNS
        dnsServers += QJsonObject{
            {"tag", "dns-local"},
            {"address", BOX_UNDERLYING_DNS_EXPORT},
            {"detour", "direct"},
        };

        // fakedns rule
        if (dataStore->fake_dns && dataStore->spmode_vpn && !status->forTest) {
            dnsRules += QJsonObject{
                {"inbound", "tun-in"},
                {"server", "dns-fake"},
            };
        }

        dns["servers"] = dnsServers;
        dns["rules"] = dnsRules;
        dns["independent_cache"] = true;

        if (dataStore->routing->use_dns_object) {
            dns = QString2QJsonObject(dataStore->routing->dns_object);
        }

        // experimental
        QJsonObject experimentalObj;

        if (!status->forTest && dataStore->core_box_clash_api > 0) {
            QJsonObject clash_api = {
                {"external_controller", NekoGui::dataStore->core_box_clash_listen_addr + ":" + Int2String(dataStore->core_box_clash_api)},
                {"secret", dataStore->core_box_clash_api_secret},
                {"external_ui", "dashboard"},
            };
            experimentalObj["clash_api"] = clash_api;
        }

        status->result->coreConfig.insert("log", QJsonObject{{"level", dataStore->log_level}});
        status->result->coreConfig.insert("dns", dns);
        status->result->coreConfig.insert("inbounds", status->inbounds);
        status->result->coreConfig.insert("outbounds", status->outbounds);
        status->result->coreConfig.insert("route", routeObj);
        if (!experimentalObj.isEmpty()) status->result->coreConfig.insert("experimental", experimentalObj);
    }
} // namespace NekoGui