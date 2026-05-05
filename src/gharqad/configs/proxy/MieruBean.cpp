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


    QString MieruBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("mierus");
        add_default_fields(url, this);
        QUrlQuery q;
        add_username_password(url, this);
        add_query_nonempty( "transport", q, QString(*network).toUpper());
        add_query_nonempty( "multiplexing", q, *multiplexing);
        add_query_nonempty( "server_ports", q, serverPorts.join(","));
        add_query_nonempty("traffic_pattern", q, traffic_pattern);
        url.setQuery(q);

        return url.toString(QUrl::FullyEncoded);
    }
}