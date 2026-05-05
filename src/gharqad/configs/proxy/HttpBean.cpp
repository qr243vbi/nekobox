#ifdef _WIN32
#include <winsock2.h>
#endif

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