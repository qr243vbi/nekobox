#include "include/configs/outbounds/tailscale.h"

#include <QJsonArray>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool tailscale::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        commons->ParseFromLink(link);

        if (query.hasQueryItem("state_directory")) state_directory = QUrl::fromPercentEncoding(query.queryItemValue("state_directory").toUtf8());
        if (query.hasQueryItem("auth_key")) auth_key = QUrl::fromPercentEncoding(query.queryItemValue("auth_key").toUtf8());
        if (query.hasQueryItem("control_url")) control_url = QUrl::fromPercentEncoding(query.queryItemValue("control_url").toUtf8());
        if (query.hasQueryItem("ephemeral")) ephemeral = query.queryItemValue("ephemeral") == "true";
        if (query.hasQueryItem("hostname")) hostname = QUrl::fromPercentEncoding(query.queryItemValue("hostname").toUtf8());
        if (query.hasQueryItem("accept_routes")) accept_routes = query.queryItemValue("accept_routes") == "true";
        if (query.hasQueryItem("exit_node")) exit_node = query.queryItemValue("exit_node");
        if (query.hasQueryItem("exit_node_allow_lan_access")) exit_node_allow_lan_access = query.queryItemValue("exit_node_allow_lan_access") == "true";
        if (query.hasQueryItem("advertise_routes")) {
            advertise_routes = QUrl::fromPercentEncoding(query.queryItemValue("advertise_routes").toUtf8()).split(",");
        }
        if (query.hasQueryItem("advertise_exit_node")) advertise_exit_node = query.queryItemValue("advertise_exit_node") == "true";
        if (query.hasQueryItem("globalDNS")) globalDNS = query.queryItemValue("globalDNS") == "true";
        if (query.hasQueryItem("global_dns")) globalDNS = query.queryItemValue("global_dns") == "true";

        return true;
    }

    bool tailscale::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "tailscale") return false;
        commons->ParseFromJson(object);
        if (object.contains("state_directory")) state_directory = object["state_directory"].toString();
        if (object.contains("auth_key")) auth_key = object["auth_key"].toString();
        if (object.contains("control_url")) control_url = object["control_url"].toString();
        if (object.contains("ephemeral")) ephemeral = object["ephemeral"].toBool();
        if (object.contains("hostname")) hostname = object["hostname"].toString();
        if (object.contains("accept_routes")) accept_routes = object["accept_routes"].toBool();
        if (object.contains("exit_node")) exit_node = object["exit_node"].toString();
        if (object.contains("exit_node_allow_lan_access")) exit_node_allow_lan_access = object["exit_node_allow_lan_access"].toBool();
        if (object.contains("advertise_routes")) {
            advertise_routes = QJsonArray2QListString(object["advertise_routes"].toArray());
        }
        if (object.contains("advertise_exit_node")) advertise_exit_node = object["advertise_exit_node"].toBool();
        if (object.contains("globalDNS")) globalDNS = object["globalDNS"].toBool();
        return true;
    }

    QString tailscale::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("ts");
        url.setHost("tailscale");
        if (!commons->name.isEmpty()) url.setFragment(commons->name);

        if (!state_directory.isEmpty()) query.addQueryItem("state_directory", QUrl::toPercentEncoding(state_directory));
        if (!auth_key.isEmpty()) query.addQueryItem("auth_key", QUrl::toPercentEncoding(auth_key));
        if (!control_url.isEmpty()) query.addQueryItem("control_url", QUrl::toPercentEncoding(control_url));
        if (ephemeral) query.addQueryItem("ephemeral", "true");
        if (!hostname.isEmpty()) query.addQueryItem("hostname", QUrl::toPercentEncoding(hostname));
        if (accept_routes) query.addQueryItem("accept_routes", "true");
        if (!exit_node.isEmpty()) query.addQueryItem("exit_node", exit_node);
        if (exit_node_allow_lan_access) query.addQueryItem("exit_node_allow_lan_access", "true");
        if (!advertise_routes.isEmpty()) query.addQueryItem("advertise_routes", QUrl::toPercentEncoding(advertise_routes.join(",")));
        if (advertise_exit_node) query.addQueryItem("advertise_exit_node", "true");
        if (globalDNS) query.addQueryItem("global_dns", "true");
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString();
    }

    QJsonObject tailscale::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "tailscale";
        mergeJsonObjects(object, commons->ExportToJson());
        if (!state_directory.isEmpty()) object["state_directory"] = state_directory;
        if (!auth_key.isEmpty()) object["auth_key"] = auth_key;
        if (!control_url.isEmpty()) object["control_url"] = control_url;
        if (ephemeral) object["ephemeral"] = ephemeral;
        if (!hostname.isEmpty()) object["hostname"] = hostname;
        if (accept_routes) object["accept_routes"] = accept_routes;
        if (!exit_node.isEmpty()) object["exit_node"] = exit_node;
        if (exit_node_allow_lan_access) object["exit_node_allow_lan_access"] = exit_node_allow_lan_access;
        if (!advertise_routes.isEmpty()) object["advertise_routes"] = QListStr2QJsonArray(advertise_routes);
        if (advertise_exit_node) object["advertise_exit_node"] = advertise_exit_node;
        if (globalDNS) object["globalDNS"] = globalDNS;
        return object;
    }

    BuildResult tailscale::Build()
    {
        QJsonObject object;
        object["type"] = "tailscale";
        mergeJsonObjects(object, commons->Build().object);
        if (!state_directory.isEmpty()) object["state_directory"] = state_directory;
        if (!auth_key.isEmpty()) object["auth_key"] = auth_key;
        if (!control_url.isEmpty()) object["control_url"] = control_url;
        if (ephemeral) object["ephemeral"] = ephemeral;
        if (!hostname.isEmpty()) object["hostname"] = hostname;
        if (accept_routes) object["accept_routes"] = accept_routes;
        if (!exit_node.isEmpty()) object["exit_node"] = exit_node;
        if (exit_node_allow_lan_access) object["exit_node_allow_lan_access"] = exit_node_allow_lan_access;
        if (!advertise_routes.isEmpty()) object["advertise_routes"] = QListStr2QJsonArray(advertise_routes);
        if (advertise_exit_node) object["advertise_exit_node"] = advertise_exit_node;
        if (globalDNS) object["globalDNS"] = globalDNS;
        return {object, ""};
    }

    QString tailscale::DisplayAddress()
    {
        return control_url;
    }

    QString tailscale::DisplayType()
    {
        return "Tailscale";
    }

    bool tailscale::IsEndpoint()
    {
        return true;
    }
}
