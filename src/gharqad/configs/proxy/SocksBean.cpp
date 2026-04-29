#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {



CoreObjOutboundBuildResult SocksBean::BuildCoreObjSingBox() const {
    CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;

    QJsonObject outbound;
    outbound["version"] = QString::number(socks_http_type);
    add_default_fields(outbound, this);
    add_username_password(outbound, this);
    add_udp_over_tcp(outbound, this);
    add_network(outbound, this);

    result.outbound = outbound;
    return result;
}

}