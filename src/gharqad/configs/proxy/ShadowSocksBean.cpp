#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {


    CoreObjOutboundBuildResult ShadowSocksBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        outbound["method"] = method;
        outbound["password"] = password;
        add_network(outbound, this);
        add_udp_over_tcp(outbound, this);

        if (!plugin.trimmed().isEmpty()) {
            outbound["plugin"] = SubStrBefore(plugin, ";");
            outbound["plugin_opts"] = SubStrAfter(plugin, ";");
        }

   //     stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }


    QString ShadowSocksBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("ss");
        if (method.startsWith("2022-")) {
            url.setUserName(fixShadowsocksUserNameEncodeMagic);
        } else {
            auto method_password = method + ":" + password;
            url.setUserName(method_password.toUtf8().toBase64(QByteArray::Base64Option::Base64UrlEncoding));
        }
        add_default_fields(url, this);
        QUrlQuery query;
        add_query_nonempty("plugin", query, plugin);

        // mux
        add_mux_state(query, this);
        // uot
        add_udp_over_tcp(query, this);

        add_network(query, this);
        if (!query.isEmpty()) url.setQuery(query);
        //
        auto link = url.toString(QUrl::FullyEncoded);
        link = link.replace(fixShadowsocksUserNameEncodeMagic, method + ":" + QUrl::toPercentEncoding(password));
        return link;
    }

}