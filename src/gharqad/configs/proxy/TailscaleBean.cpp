



#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {

    CoreObjOutboundBuildResult TailscaleBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QJsonObject outbound{
            {"type", this->type()},
            {"state_directory", state_directory},
            {"auth_key", auth_key},
            {"control_url", control_url},
            {"ephemeral", ephemeral},
            {"hostname", hostname},
            {"accept_routes", accept_routes},
            {"exit_node", exit_node},
            {"exit_node_allow_lan_access", exit_node_allow_lan_access},
            {"advertise_routes", QListStr2QJsonArray(advertise_routes)},
            {"advertise_exit_node", advertise_exit_node},
        };

        result.outbound = outbound;
        return result;
    }



    QString TailscaleBean::ToShareLink() const
    {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("ts");
        url.setHost("tailscale");
        if (!entity->name.isEmpty()) url.setFragment(entity->name);
        QUrlQuery q;
        add_query_nonempty("state_directory", q, QUrl::toPercentEncoding(state_directory));
        add_query_nonempty("auth_key", q, QUrl::toPercentEncoding(auth_key));
        add_query_nonempty("control_url", q, QUrl::toPercentEncoding(control_url));
        add_query_boolean("ephemeral", q, ephemeral);
        add_query_nonempty("hostname", q, QUrl::toPercentEncoding(hostname));
        add_query_boolean("accept_routes", q, accept_routes);
        add_query_nonempty("exit_node", q, exit_node);
        add_query_boolean("exit_node_allow_lan_access", q, exit_node_allow_lan_access);
        add_query_nonempty("advertise_routes", q, QUrl::toPercentEncoding(advertise_routes.join(",")));
        add_query_boolean("advertise_exit_node", q, advertise_exit_node);
        add_query_boolean("global_dns", q, globalDNS);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }    
    bool TailscaleBean::TryParseJson(const Configs::Data::Node& obj)
    {
        entity->name = obj["tag"].toString();
        state_directory = obj["state_directory"].toString();
        auth_key = obj["auth_key"].toString();
        control_url = obj["control_url"].toString();
        ephemeral = obj["ephemeral"].toBool();
        hostname = obj["hostname"].toString();
        accept_routes = obj["accept_routes"].toBool();
        exit_node = obj["exit_node"].toString();
        exit_node_allow_lan_access = obj["exit_node_allow_lan_access"].toBool();
        advertise_routes = obj["advertise_routes"].toStringList();
        advertise_exit_node = obj["advertise_exit_node"].toBool();

        return true;
    }  
    bool TailscaleBean::TryParseYaml(const Configs::Data::Node& obj)
    {
        entity->name = obj["name"].toString();
        state_directory = obj["state-dir"].toString();
        auth_key = obj["auth-key"].toString();
        control_url = obj["control-url"].toString();
        ephemeral = obj["ephemeral"].toBool();
        hostname = obj["hostname"].toString();
        accept_routes = obj["accept-routes"].toBool();
        exit_node = obj["exit-node"].toString();
        exit_node_allow_lan_access = obj["exit-node-allow-lan-access"].toBool();

        return true;
    }
        bool TailscaleBean::TryParseLink(const QString &link)
    {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        entity->name = url.fragment(QUrl::FullyDecoded);

        state_directory = QUrl::fromPercentEncoding(query.queryItemValue("state_directory").toUtf8());
        auth_key = QUrl::fromPercentEncoding(query.queryItemValue("auth_key").toUtf8());
        control_url = QUrl::fromPercentEncoding(query.queryItemValue("control_url").toUtf8());
        set_boolean("ephemeral", ephemeral, query);
        hostname = QUrl::fromPercentEncoding(query.queryItemValue("hostname").toUtf8());
        set_boolean("accept_routes", accept_routes, query);
        exit_node = query.queryItemValue("exit_node");
        set_boolean("exit_node_allow_lan_access", exit_node_allow_lan_access, query);
        advertise_routes = QUrl::fromPercentEncoding(query.queryItemValue("advertise_routes").toUtf8()).split(",");
        set_boolean("advertise_exit_node", advertise_exit_node, query);
        set_boolean("globalDNS", globalDNS, query);
        set_boolean("global_dns", globalDNS, query);

        return true;
    }
}