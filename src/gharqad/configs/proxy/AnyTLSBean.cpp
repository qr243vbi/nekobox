#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    
    CoreObjOutboundBuildResult AnyTLSBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound{
            {"password", password},
            {"idle_session_check_interval", idle_session_check_interval},
            {"idle_session_timeout", idle_session_timeout},
            {"min_idle_session", min_idle_session},
        };
        add_default_fields(outbound, this);
        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

}