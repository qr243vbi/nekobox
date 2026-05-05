#ifdef _WIN32
#include <winsock2.h>
#endif

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
}