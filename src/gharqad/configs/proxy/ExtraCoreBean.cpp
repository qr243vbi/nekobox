#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {

    CoreObjOutboundBuildResult ExtraCoreBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QJsonObject outbound{
            {"type", "socks"},
            {"server", socksAddress},
            {"server_port", socksPort},
        };
        result.outbound = outbound;
        return result;
    }

}