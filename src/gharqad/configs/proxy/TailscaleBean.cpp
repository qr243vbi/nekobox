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
}