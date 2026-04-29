#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {

    
    CoreObjOutboundBuildResult MieruBean::BuildCoreObjSingBox() const
    {
        using namespace To_CoreObj_box;
        CoreObjOutboundBuildResult result;
        QJsonObject outbound {
            {"server_ports", QListStr2QJsonArray(this->serverPorts)},
            {"transport", QString(*this->network).toUpper()},
            {"multiplexing", *this->multiplexing},
        };
        add_username_password(outbound, this);
        add_default_fields(outbound, this);
        add_non_empty(outbound, "traffic_pattern", traffic_pattern);
        result.outbound = outbound;
        return result;
    }

}