#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    
    CoreObjOutboundBuildResult HttpBean::BuildCoreObjSingBox() const {
        using namespace To_CoreObj_box;
        CoreObjOutboundBuildResult result;

        QJsonObject outbound;
        add_default_fields(outbound, this);
        add_non_empty(outbound, "path", path);
        add_username_password(outbound, this);
        add_non_empty(outbound, "headers", this->headers);

        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }
}