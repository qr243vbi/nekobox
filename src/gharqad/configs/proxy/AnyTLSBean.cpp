#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    

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