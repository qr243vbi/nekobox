#pragma once

#include "main/NekoGui.hpp"

namespace NekoGui {
    class RouteRule : public JsonStore {
    public:
        int ip_version = 0;
        QList<QString> network;
        QList<QString> protocol;
        QList<QString> domain;
        QList<QString> domain_suffix;
        QList<QString> domain_keyword;
        QList<QString> domain_regex;
        QList<QString> source_ip_cidr;
        bool* source_ip_is_private = nullptr;
        QList<QString> ip_cidr;
        bool* ip_is_private = nullptr;
        QList<QString> source_port;
        QList<QString> source_port_range;
        QList<QString> port;
        QList<QString> port_range;
        QList<QString> process_name;
        QList<QString> process_path;
        QList<QString> rule_set;
        bool invert = false;
        int outboundID = -1;

        QList<QString> check_for_errors();

        QJsonObject get_rule_json();
    };

    class RoutingChain : public JsonStore {
    public:
        int id = -1;
        QString name = "";
        QList<std::shared_ptr<RouteRule>> Rules;

        QJsonArray get_route_rules();

        QJsonArray get_default_route_rules();
    };
} // namespace NekoGui