#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    

    CoreObjOutboundBuildResult TrustTunnelBean::BuildCoreObjSingBox() const
    {
        using namespace To_CoreObj_box;
        CoreObjOutboundBuildResult result;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        add_username_password(outbound, this);
        add_quic(outbound, this);
        outbound["health_check"] = this->health_check;
        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }


    QString TrustTunnelBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("tt");
        add_default_fields(url, this);
        QUrlQuery q;
        add_username_password(url, this);
        add_quic(q, this);
        add_tls(stream, q);
        add_query_boolean("health_check", q, health_check);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }
}