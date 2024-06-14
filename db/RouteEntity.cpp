#include <QJsonObject>
#include <QJsonArray>
#include "RouteEntity.h"
#include "db/Database.hpp"

namespace NekoGui {
    QJsonArray get_as_array(const QList<QString>& str, bool castToNum = false) {
        QJsonArray res;
        for (const auto &item: str) {
            if (item.trimmed().isEmpty()) continue;
            if (castToNum) res.append(item.toInt());
            else res.append(item);
        }
        return res;
    }

    bool isValidStrArray(const QStringList& arr) {
        for (const auto& item: arr) {
            if (!item.trimmed().isEmpty()) return true;
        }
        return false;
    }

    QJsonObject RouteRule::get_rule_json(bool forView, const QString& outboundTag) const {
        QJsonObject obj;

        if (ip_version != "") obj["ip_version"] = ip_version.toInt();
        if (network != "") obj["network"] = network;
        if (protocol != "") obj["protocol"] = protocol;
        if (isValidStrArray(domain)) obj["domain"] = get_as_array(domain);
        if (isValidStrArray(domain_suffix)) obj["domain_suffix"] = get_as_array(domain_suffix);
        if (isValidStrArray(domain_keyword)) obj["domain_keyword"] = get_as_array(domain_keyword);
        if (isValidStrArray(domain_regex)) obj["domain_regex"] = get_as_array(domain_regex);
        if (isValidStrArray(source_ip_cidr)) obj["source_ip_cidr"] = get_as_array(source_ip_cidr);
        if (source_ip_is_private) obj["source_ip_is_private"] = source_ip_is_private;
        if (isValidStrArray(ip_cidr)) obj["ip_cidr"] = get_as_array(ip_cidr);
        if (ip_is_private) obj["ip_is_private"] = ip_is_private;
        if (isValidStrArray(source_port)) obj["source_port"] = get_as_array(source_port, true);
        if (isValidStrArray(source_port_range)) obj["source_port_range"] = get_as_array(source_port_range);
        if (isValidStrArray(port)) obj["port"] = get_as_array(port, true);
        if (isValidStrArray(port_range)) obj["port_range"] = get_as_array(port_range);
        if (isValidStrArray(process_name)) obj["process_name"] = get_as_array(process_name);
        if (isValidStrArray(process_path)) obj["process_path"] = get_as_array(process_path);
        if (isValidStrArray(rule_set)) obj["rule_set"] = get_as_array(rule_set);
        if (invert) obj["invert"] = invert;

        if (forView) {
            switch (outboundID) { // TODO use constants
                case -1:
                    obj["outbound"] = "proxy";
                    break;
                case -2:
                    obj["outbound"] = "direct";
                    break;
                case -3:
                    obj["outbound"] = "block";
                    break;
                case -4:
                    obj["outbound"] = "dns_out";
                    break;
                default:
                    auto prof = NekoGui::profileManager->GetProfile(outboundID);
                    if (prof == nullptr) {
                        MW_show_log("The outbound described in the rule chain is missing, maybe your data is corrupted");
                        return {};
                    }
                    obj["outbound"] = prof->bean->DisplayName();
            }
        } else {
            if (!outboundTag.isEmpty()) obj["outbound"] = outboundTag;
            else obj["outbound"] = outboundID;
        }

        return obj;
    }

    // TODO use constant for field names
    QStringList RouteRule::get_attributes() {
        return {
                "ip_version",
                "network",
                "protocol",
                "domain",
                "domain_suffix",
                "domain_keyword",
                "domain_regex",
                "source_ip_cidr",
                "source_ip_is_private",
                "ip_cidr",
                "ip_is_private",
                "source_port",
                "source_port_range",
                "port",
                "port_range",
                "process_name",
                "process_path",
                "rule_set",
                "invert",
        };
    }

    inputType RouteRule::get_input_type(const QString& fieldName) {
        if (fieldName == "invert" ||
            fieldName == "source_ip_is_private" ||
            fieldName == "ip_is_private") return trufalse;

        if (fieldName == "ip_version" ||
            fieldName == "network" ||
            fieldName == "protocol") return select;

        return text;
    }

    QStringList RouteRule::get_values_for_field(const QString& fieldName) {
        if (fieldName == "ip_version") {
            return {"", "4", "6"};
        }
        if (fieldName == "network") {
            return {"", "tcp", "udp"};
        }
        if (fieldName == "protocol") {
            return {"", "http", "tls", "quic", "stun", "dns", "bittorrent"};
        }
        return {};
    }

    QStringList RouteRule::get_current_value_string(const QString& fieldName) {
        if (fieldName == "ip_version") {
            return {ip_version};
        }
        if (fieldName == "network") {
            return {network};
        }
        if (fieldName == "protocol") {
            return {protocol};
        }
        if (fieldName == "domain") return domain;
        if (fieldName == "domain_suffix") return domain_suffix;
        if (fieldName == "domain_keyword") return domain_keyword;
        if (fieldName == "domain_regex") return domain_regex;
        if (fieldName == "source_ip_cidr") return source_ip_cidr;
        if (fieldName == "ip_cidr") return ip_cidr;
        if (fieldName == "source_port") return source_port;
        if (fieldName == "source_port_range") return source_port_range;
        if (fieldName == "port") return port;
        if (fieldName == "port_range") return port_range;
        if (fieldName == "process_name") return process_name;
        if (fieldName == "process_path") return process_path;
        if (fieldName == "rule_set") return rule_set;
        return {};
    }

    QString RouteRule::get_current_value_bool(const QString& fieldName) const {
        if (fieldName == "source_ip_is_private") {
            return source_ip_is_private? "true":"false";
        }
        if (fieldName == "ip_is_private") {
            return ip_is_private? "true":"false";
        }
        if (fieldName == "invert") {
            return invert? "true":"false";
        }
        return nullptr;
    }

    void RouteRule::set_field_value(const QString& fieldName, const QStringList& value) {
        if (fieldName == "ip_version") {
            ip_version = value[0];
        }
        if (fieldName == "network") {
            network = value[0];
        }
        if (fieldName == "protocol") {
            protocol = value[0];
        }
        if (fieldName == "domain") {
            domain = value;
        }
        if (fieldName == "domain_suffix") {
            domain_suffix = value;
        }
        if (fieldName == "domain_keyword") {
            domain_keyword = value;
        }
        if (fieldName == "domain_regex") {
            domain_regex = value;
        }
        if (fieldName == "source_ip_cidr") {
            source_ip_cidr = value;
        }
        if (fieldName == "source_ip_is_private") {
            source_ip_is_private = value[0]=="true";
        }
        if (fieldName == "ip_cidr") {
            ip_cidr = value;
        }
        if (fieldName == "ip_is_private") {
            ip_is_private = value[0]=="true";
        }
        if (fieldName == "source_port") {
            source_port = value;
        }
        if (fieldName == "source_port_range") {
            source_port_range = value;
        }
        if (fieldName == "port") {
            port = value;
        }
        if (fieldName == "port_range") {
            port_range = value;
        }
        if (fieldName == "process_name") {
            process_name = value;
        }
        if (fieldName == "process_path") {
            process_path = value;
        }
        if (fieldName == "rule_set") {
            rule_set = value;
        }
        if (fieldName == "invert") {
            invert = value[0]=="true";
        }
    }

    bool RouteRule::isEmpty() const {
        return get_rule_json().keys().length() == 1;
    }

    QJsonArray RoutingChain::get_route_rules(bool forView, std::map<int, QString> outboundMap) {
        QJsonArray res;
        for (const auto &item: Rules) {
            auto outboundTag = QString();
            if (outboundMap.count(item->outboundID)) outboundTag = outboundMap[item->outboundID];
            auto rule_json = item->get_rule_json(forView, outboundTag);
            if (rule_json.empty()) {
                MW_show_log("Aborted generating routing section, an error has occurred");
                return {};
            }
            res += rule_json;
        }

        return res;
    }

    std::shared_ptr<RoutingChain> RoutingChain::GetDefaultChain() {
        auto defaultChain = std::make_shared<RoutingChain>();
        defaultChain->name = "Default";
        auto defaultRule = std::make_shared<RouteRule>();
        defaultRule->protocol = "dns";
        defaultRule->outboundID = -4;
        defaultChain->Rules << defaultRule;
        return defaultChain;
    }

    std::shared_ptr<QList<int>> RoutingChain::get_used_outbounds() {
        auto res = std::make_shared<QList<int>>();
        for (const auto& item: Rules) {
            res->push_back(item->outboundID);
        }
        return res;
    }

    std::shared_ptr<QStringList> RoutingChain::get_used_rule_sets() {
        auto res = std::make_shared<QStringList>();
        for (const auto& item: Rules) {
            for (const auto& ruleItem: item->rule_set) {
                res->push_back(ruleItem);
            }
        }
        return res;
    }

    bool RoutingChain::Save() {
        castedRules.clear();
        for (const auto &item: Rules) {
            castedRules.push_back(dynamic_cast<JsonStore*>(item.get()));
        }
        return JsonStore::Save();
    }

    void RoutingChain::FromJson(QJsonObject object) {
        for (const auto &key: object.keys()) {
            if (_map.count(key) == 0) {
                continue;
            }

            auto value = object[key];
            auto item = _map[key].get();

            if (item == nullptr) continue;
            if (item->type == itemType::jsonStoreList) {
                // it is of rule type
                if (!value.isArray()) continue;
                Rules.clear();
                auto arr = value.toArray();
                for (auto obj : arr) {
                    auto rule = std::make_shared<RouteRule>();
                    rule->FromJson(obj.toObject());
                    Rules << rule;
                }
            }
        }
        JsonStore::FromJson(object);
    }
}