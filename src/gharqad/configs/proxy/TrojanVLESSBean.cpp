#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {



    CoreObjOutboundBuildResult TrojanVLESSBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        add_network(outbound, this);
        QString flow = this->flow;
        if (proxy_type == proxy_VLESS) {
            if (flow.right(7) == "-udp443") {
                flow.chop(7);
            } else if (flow == "none") {
                // 不使用 flow
                flow = "";
            }
            outbound["uuid"] = password.trimmed();
            outbound["flow"] = flow;
            add_non_empty(outbound, "encryption", encryption);
        } else {
            outbound["password"] = password;
        }

        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

    QString TrojanVLESSBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        QUrlQuery query;
        url.setScheme(proxy_type == proxy_VLESS ? "vless" : "trojan");
        url.setUserName(password);
        add_default_fields(url, this);
        add_network(query, this);

        //  security
        add_tls(stream, query);

        // mux
        add_mux_state(query, this);

        // protocol
        if (proxy_type == proxy_VLESS) {
            add_query_nonempty("flow", query, flow);
            add_query_nonempty("packetEncoding", query, *stream->packet_encoding);
            query.addQueryItem("encryption", (encryption == "" ) ? "none" : encryption);
        }

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

}