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

    QString JuicityBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("juicity");
        add_default_fields(url, this);
        QUrlQuery q;
        add_username_password(url, this);
        add_tls(stream, q);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }
}