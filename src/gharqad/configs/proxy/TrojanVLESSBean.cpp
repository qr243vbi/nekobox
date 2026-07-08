



#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {


    bool TrojanVLESSBean::TryParseJson(const Configs::Data::Node& obj)
    {
        using namespace Configs::From_Json;
        proxy_type = obj["type"].toString() == "trojan" ? proxy_Trojan : proxy_VLESS;
        add_default_fields(this->entity, obj);
        password = obj["password"].toString();
        if (proxy_type == proxy_VLESS) password = obj["uuid"].toString();
        flow = obj["flow"].toString();
        encryption = obj["encryption"].toString();
        add_mux_state(this, obj);

        *stream->packet_encoding = obj["packet_encoding"].toString();

        add_tls(stream, obj);
        parse_transport(stream, obj);
        add_network(this, obj);
        return true;
    }

    bool TrojanVLESSBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        password = url.userName();
        if (entity->serverPort == -1) entity->serverPort = 443;

        add_network(this, query);
        // security

        auto network =  GetQueryValue(query, "type", "tcp");
        if (network == "h2") {
            network = "http";
        }
        *stream->network = network;

        if (proxy_type == proxy_Trojan) {
            stream->security = GetQueryValue(query, "security", "tls").replace("reality", "tls").replace("none", "");
        } else {
            stream->security = GetQueryValue(query, "security", "").replace("reality", "tls").replace("none", "");
        }
        auto sni1 = GetQueryValue(query, "sni");
        auto sni2 = GetQueryValue(query, "peer");
        if (!sni1.isEmpty()) stream->sni = sni1;
        if (!sni2.isEmpty()) stream->sni = sni2;
        stream->alpn = GetQueryValue(query, "alpn");
        if (!query.queryItemValue("allowInsecure").isEmpty()) stream->allow_insecure = true;
        stream->reality_pbk = GetQueryValue(query, "pbk", "");
        stream->reality_sid = GetQueryValue(query, "sid", "");
        stream->utlsFingerprint = GetQueryValue(query, "fp", "");
        if (query.queryItemValue("fragment") == "1") stream->enable_tls_fragment = true;
        stream->tls_fragment_fallback_delay = query.queryItemValue("fragment_fallback_delay");
        if (query.queryItemValue("record_fragment") == "1") stream->enable_tls_record_fragment = true;
        if (stream->utlsFingerprint.isEmpty()) {
            stream->utlsFingerprint = dataStore->utlsFingerprint;
        }
        if (stream->security.isEmpty()) {
            if (!sni1.isEmpty() || !sni2.isEmpty()) stream->security = "tls";
        }

        // type
        if (network == "ws") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "");
        } else if (network == "http") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "").replace("|", ",");
            stream->method = GetQueryValue(query, "method", "");
        } else if (network == "httpupgrade") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "");
        } else if (network == "grpc") {
            stream->path = GetQueryValue(query, "serviceName", "");
        } else if (network == "tcp") {
            if (GetQueryValue(query, "headerType") == "http") {
                stream->header_type = "http";
                stream->host = GetQueryValue(query, "host", "");
                stream->path = GetQueryValue(query, "path", "");
            }
        } else if (network == "xhttp") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "");
            stream->xhttp_mode = GetQueryValue(query, "mode", "auto");
            stream->xhttp_extra = GetQueryValue(query, "extra", "");
        }

        // mux
        add_mux_state(this, query);

        // protocol
        if (proxy_type == proxy_VLESS) {
            flow = GetQueryValue(query, "flow", "");
            encryption = GetQueryValue(query, "encryption", "");
            *stream->packet_encoding = GetQueryValue(query, "packetEncoding", "xudp");
        }

        return !(password.isEmpty() || entity->serverAddress.isEmpty());
    }


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