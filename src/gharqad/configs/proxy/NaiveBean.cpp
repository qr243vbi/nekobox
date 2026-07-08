



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
    bool NaiveBean::TryParseJson(const Configs::Data::Node& obj)
    {
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        add_username_password(this, obj);
        insecure_concurrency = obj["insecure_concurrency"].toInt();
        extra_headers = obj["extra_headers"].toVariantMap();
        add_udp_over_tcp(this, obj);
        add_quic(this, obj);
        add_tls(stream, obj);
        return true;
    }

    bool NaiveBean::TryParseLink(const QString& link)
    {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        QUrlQuery query = GetQuery(url);
        add_default_fields(url, entity);

        add_username_password(this, url);
        add_quic(this, query);
        add_udp_over_tcp(this, query);
        extra_headers = GetQueryMapValue(query, "extra_headers");

        add_tls(stream, query);
        return true;
    }
}