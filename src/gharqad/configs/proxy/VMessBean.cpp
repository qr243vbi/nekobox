#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {


    CoreObjOutboundBuildResult VMessBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound{
            {"uuid", uuid.trimmed()},
            {"alter_id", aid},
            {"security", security},
            {"authenticated_length", authenticated_length},
            {"global_padding", global_padding}
        };
        add_network(outbound, this);
        add_default_fields(outbound, this);

        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }


    QString VMessBean::ToShareLink() const {
        QUrl url;
            using namespace Configs::To_Link;

        QUrlQuery query;
        url.setScheme("vmess");
        url.setUserName(uuid);
        add_default_fields(url, this);

        add_query_boolean("global_padding", query, this->global_padding);
        add_query_boolean("authenticated_length", query, this->authenticated_length);

        add_query_nonempty("encryption", query, security);
        add_network(query, this);

        //  security
        add_tls(stream, query);

        // mux
        add_mux_state(query, this);

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }
}