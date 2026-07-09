



#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    bool MieruBean::TryParseLink(const QString& link)
    {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        add_username_password(this, url);

        *network = query.queryItemValue("transport").toLower();
        traffic_pattern = GetQueryValue(query, "traffic_pattern");
        *multiplexing = query.queryItemValue("multiplexing");
        serverPorts = query.queryItemValue("server_ports").split(",");
        return true;
    }
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

    bool MieruBean::TryParseJson(const Configs::Data::Node& obj)
    {
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        add_username_password(this, obj);
        *network = obj["transport"].toString();
        *multiplexing = obj["multiplexing"].toString();
        traffic_pattern = obj["traffic_pattern"].toString();
        serverPorts = obj["server_ports"].toStringList();
        return true;
    }

    bool MieruBean::TryParseYaml(const Configs::Data::Node& obj)
    {
        using namespace Configs::From_Yaml;
        add_default_fields(this->entity, obj);
        add_username_password(this, obj);
        *network = obj["transport"].toString();
        *multiplexing = obj["multiplexing"].toString();
        traffic_pattern = obj["traffic-pattern"].toString();
        serverPorts = obj["port-range"].toStringList();
        return true;
    }
}