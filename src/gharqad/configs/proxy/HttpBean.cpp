



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

    bool HttpBean::TryParseJson(const Configs::Data::Node & obj)
    {
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        add_username_password(this, obj);
        path = obj["path"].toString();
        headers = obj["headers"].toVariantMap();
        add_tls(stream, obj);
        return true;
    }

    bool HttpBean::TryParseYaml(const Configs::Data::Node & obj)
    {
        using namespace Configs::From_Yaml;
        add_default_fields(this->entity, obj);
        add_username_password(this, obj);
        path = obj["path"].toString();
        headers = obj["headers"].toVariantMap();
        add_tls(stream, obj);
        return true;
    }

    bool HttpBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        add_default_fields(url, entity);
        if (entity->serverPort == -1) entity->serverPort = 443;
        add_username_password(this, url);
        path = url.path();
        headers = GetQueryMapValue(query, "headers");
        if (link.startsWith("https")) {
            add_tls(stream, query);
        };
        return !entity->serverAddress.isEmpty();
    }
    
    QString HttpBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        QUrlQuery query;
        add_default_fields(url, this);
        { // http
            if (stream->security == "tls") {
                url.setScheme("https");
            } else {
                url.setScheme("http");
            }
        }
        add_username_password(url, this);
        if (!path.isEmpty()) url.setPath(path);
        add_query_map_nonempty("headers", query, headers);
        add_tls(stream, query);
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

}