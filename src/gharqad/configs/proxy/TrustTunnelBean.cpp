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
    bool TrustTunnelBean::TryParseJson(const QJsonObject& obj)
    {
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        add_username_password(this, obj);
        add_quic(this, obj);
        add_tls(stream, obj);
        health_check = obj["health_check"].toBool();
        return true;
    }    
    bool TrustTunnelBean::TryParseLink(const QString& link)
    {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        QUrlQuery query = GetQuery(url);
        add_default_fields(url, entity);

        add_username_password(this, url);
        add_quic(this, query);
        set_boolean("health_check", this->health_check, query);

        add_tls(stream, query);
        return true;
    }
}