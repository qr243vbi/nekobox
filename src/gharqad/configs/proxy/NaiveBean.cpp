#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {



    CoreObjOutboundBuildResult NaiveBean::BuildCoreObjSingBox() const
    {
        using namespace To_CoreObj_box;
        CoreObjOutboundBuildResult result;
        QJsonObject outbound {
            {"insecure_concurrency", this->insecure_concurrency},
            {"extra_headers", QJsonObject::fromVariantMap(this->extra_headers)},
        };
        add_default_fields(outbound, this);
        add_username_password(outbound, this);
        add_udp_over_tcp(outbound, this);
        add_quic(outbound, this);

        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

    QString NaiveBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("naive");
        add_default_fields(url, this);
        QUrlQuery q;
        add_username_password(url, this);
        add_quic(q, this);
        add_udp_over_tcp(q, this);
        add_query_map_nonempty("extra_headers", q, extra_headers);
        add_tls(stream, q);

        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }
}