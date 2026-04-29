#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {


    CoreObjOutboundBuildResult JuicityBean::BuildCoreObjSingBox() const
    {
        using namespace To_CoreObj_box;
        CoreObjOutboundBuildResult result;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        outbound["uuid"] = this->username;
        outbound["password"] = this->password;
        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }
}