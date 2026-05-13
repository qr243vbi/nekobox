

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    

    bool AnyTLSBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        password = url.userName();
        if (entity->serverPort == -1) entity->serverPort = 443;
        this->idle_session_check_interval = GetQueryValue(query, "idle_session_check_interval", "30s");
        this->idle_session_timeout = GetQueryValue(query, "idle_session_timeout", "30s");
        this->min_idle_session = GetQueryIntValue(query, "min_idle_session", 0);
        // security
        add_tls(stream, query);

        return !(password.isEmpty() || entity->serverAddress.isEmpty());
    }


    QString AnyTLSBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        QUrlQuery query;
        url.setScheme("anytls");
        url.setUserName(password);        
        add_default_fields(url, this);
        add_query_nonempty("idle_session_check_interval", query, idle_session_check_interval);
        add_query_nonempty("idle_session_timeout", query, idle_session_timeout);
        add_query_int_natural("min_idle_session", query, min_idle_session);

        //  security
        add_tls(stream, query);

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }
    bool AnyTLSBean::TryParseJson(const QJsonObject& obj)
    {
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        password = obj["password"].toString();
        idle_session_check_interval = obj["idle_session_check_interval"].toString();
        idle_session_timeout = obj["idle_session_timeout"].toString();
        min_idle_session = obj["min_idle_session"].toInt();
        add_tls(stream, obj);
        return true;
    }
    CoreObjOutboundBuildResult AnyTLSBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound{
            {"password", password},
            {"idle_session_check_interval", idle_session_check_interval},
            {"idle_session_timeout", idle_session_timeout},
            {"min_idle_session", min_idle_session},
        };
        add_default_fields(outbound, this);
        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

}