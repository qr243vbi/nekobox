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

    bool VMessBean::TryParseJson(const QJsonObject& obj)
    {
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        uuid = obj["uuid"].toString();
        security = obj["security"].toString();
        aid = obj["alter_id"].toInt();
        add_mux_state(this, obj);

        *stream->packet_encoding = obj["packet_encoding"].toString();

        global_padding = obj["global_padding"].toBool();
        authenticated_length = obj["authenticated_length"].toBool();

        add_tls(stream, obj);
        parse_transport(stream, obj);
        add_network(this, obj);
        return true;
    }

        bool VMessBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        // V2RayN Format
        auto linkN = DecodeB64IfValid(SubStrAfter(link, "vmess://"));
        if (!linkN.isEmpty()) {
            auto objN = QString2QJsonObject(linkN);
            if (objN.isEmpty()) return false;
            // REQUIRED
            uuid = objN["id"].toString();
            entity->serverAddress = objN["add"].toString();
            entity->serverPort = objN["port"].toVariant().toInt();
            // OPTIONAL
            entity->name = objN["ps"].toString();
            aid = objN["aid"].toVariant().toInt();
            stream->host = objN["host"].toString();
            stream->path = objN["path"].toString();
            stream->sni = objN["sni"].toString();
            stream->header_type = objN["type"].toString();
            auto net = objN["net"].toString();
            if (!net.isEmpty()) {
                if (net == "h2") {
					net_type_ret:
                    net = "http";
                }
                *stream->network = net;
            } else if (objN["type"].toString() == "http"){
				goto net_type_ret;
			}
            auto scy = objN["scy"].toString();
            if (!scy.isEmpty()) security = scy;
            // TLS (XTLS?)
            stream->security = objN["tls"].toString();
            // TODO quic & kcp
            return true;
        } else {
            // https://github.com/XTLS/Xray-core/discussions/716
            auto url = QUrl(link);
            if (!url.isValid()) return false;
            auto query = GetQuery(url);
            add_default_fields(url, entity);
            uuid = url.userName();
            if (entity->serverPort == -1) entity->serverPort = 443;

            aid = 0; // “此分享标准仅针对 VMess AEAD 和 VLESS。”
            security = GetQueryValue(query, "encryption", "auto");
            set_boolean("global_padding", global_padding, query);
            set_boolean("authenticated_length", authenticated_length, query);
            add_network(this, query);
            // security
            auto network = GetQueryValue(query, "type", "tcp");
            if (network == "h2") {
                network = "http";
            }
            *stream->network = network;
            stream->security = GetQueryValue(query, "security", "tls").replace("reality", "tls");
            auto sni1 = GetQueryValue(query, "sni");
            auto sni2 = GetQueryValue(query, "peer");
            if (!sni1.isEmpty()) stream->sni = sni1;
            if (!sni2.isEmpty()) stream->sni = sni2;
            if (!query.queryItemValue("allowInsecure").isEmpty()) stream->allow_insecure = true;
            stream->reality_pbk = GetQueryValue(query, "pbk", "");
            stream->reality_sid = GetQueryValue(query, "sid", "");
            stream->utlsFingerprint = GetQueryValue(query, "fp", "");
            if (stream->utlsFingerprint.isEmpty()) {
                stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
            }
            if (query.queryItemValue("fragment") == "1") stream->enable_tls_fragment = true;
            stream->tls_fragment_fallback_delay = query.queryItemValue("fragment_fallback_delay");
            if (query.queryItemValue("record_fragment") == "1") stream->enable_tls_record_fragment = true;

            // mux
            add_mux_state(this, query);

            // type
            if (network == "ws") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "");
            } else if (network == "http") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "").replace("|", ",");
            } else if (network == "httpupgrade") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "");
            } else if (network == "grpc") {
                stream->path = GetQueryValue(query, "serviceName", "");
            } else if (network == "tcp") {
                if (GetQueryValue(query, "headerType") == "http") {
                    stream->header_type = "http";
                    stream->path = GetQueryValue(query, "path", "");
                    stream->host = GetQueryValue(query, "host", "");
                }
            } else if (network == "xhttp") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "");
                stream->xhttp_mode = GetQueryValue(query, "mode", "auto");
                stream->xhttp_extra = GetQueryValue(query, "extra", "");
            }
            return !(uuid.isEmpty() || entity->serverAddress.isEmpty());
        }

        return false;
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