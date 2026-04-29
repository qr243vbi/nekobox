#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {



    CoreObjOutboundBuildResult TrojanVLESSBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        add_network(outbound, this);
        QString flow = this->flow;
        if (proxy_type == proxy_VLESS) {
            if (flow.right(7) == "-udp443") {
                flow.chop(7);
            } else if (flow == "none") {
                // 不使用 flow
                flow = "";
            }
            outbound["uuid"] = password.trimmed();
            outbound["flow"] = flow;
            add_non_empty(outbound, "encryption", encryption);
        } else {
            outbound["password"] = password;
        }

        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

}