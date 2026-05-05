#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {




    QString SocksBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        QUrlQuery query;
        add_default_fields(url, this);
        {
            url.setScheme(QString("socks%1").arg(socks_http_type));
        }
        add_username_password(url, this);
        add_network(query, this);
        add_udp_over_tcp(query, this);
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }
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