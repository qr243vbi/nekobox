#pragma once

#include "main/NekoGui.hpp"

namespace NekoGui {
    enum inputType {trufalse, select, text};

    class RouteRule : public JsonStore {
    public:
        QString name = "";
        QString ip_version;
        QString network;
        QString protocol;
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
        int outboundID = -1; // -2 is direct -3 is block -4 is dns_out

        [[nodiscard]] QJsonObject get_rule_json() const;
        static QStringList get_attributes();
        static inputType get_input_type(const QString& fieldName);
        static QStringList get_values_for_field(const QString& fieldName);
        QStringList get_current_value_string(const QString& fieldName);
        [[nodiscard]] bool* get_current_value_bool(const QString& fieldName) const;
        void set_field_value(const QString& fieldName, const QStringList& value);
    };

    class RoutingChain : public JsonStore {
    public:
        int id = -1;
        QString name = "";
        QList<std::shared_ptr<RouteRule>> Rules;

        QJsonArray get_route_rules();

        static std::shared_ptr<RoutingChain> GetDefaultChain();
    };
} // namespace NekoGui