#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {


    CoreObjOutboundBuildResult ShadowSocksBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        outbound["method"] = method;
        outbound["password"] = password;
        add_network(outbound, this);
        add_udp_over_tcp(outbound, this);

        if (!plugin.trimmed().isEmpty()) {
            outbound["plugin"] = SubStrBefore(plugin, ";");
            outbound["plugin_opts"] = SubStrAfter(plugin, ";");
        }

   //     stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

}