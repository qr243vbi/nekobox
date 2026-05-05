#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {




    QString ShadowTLSBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        QUrlQuery query;
        url.setScheme("shadowtls");
        url.setUserName(password);        
        add_default_fields(url, this);
        add_query_int("version", query, shadowtls_version);
        add_tls(stream, query);
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }
    CoreObjOutboundBuildResult ShadowTLSBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound{
            {"password", password},
            {"version", shadowtls_version},
        };

        add_default_fields(outbound, this);

        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }
}