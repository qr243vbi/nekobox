#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    namespace CoreObj_box = To_CoreObj_box;
    QJsonValue CoreObj_box::udp_over_tcp_object(int version){
        QJsonValue val;
        if (version == 0){
            val = false;
        } else {
            QJsonObject udp_over_tcp{
                {"enabled", true},
                {"version", version}
            };
        }
        return val;
    }

    QJsonObject CoreObj_box::getXmux(const QJsonValue & value){
        QJsonObject obj;
        for (auto [k, v]: asKeyValueRange(value.toObject().toVariantMap()) ){
            QString key = k.toLower().replace("_", "");
            QJsonValue value = v.toJsonValue();
            if (key == "maxconcurrency"){
                obj["max_concurrency"] = getXbadoptionRange(value);
            } else if (key == "maxconnections"){
                obj["max_connections"] = getXbadoptionRange(value);
            } else if (key == "cmaxreusetimes"){
                obj["c_max_reuse_times"] = getXbadoptionRange(value);
            } else if (key == "hmaxrequesttimes"){
                obj["h_max_request_times"] = getXbadoptionRange(value);
            } else if (key == "hmaxreusablesecs"){
                obj["h_max_reusable_secs"] = getXbadoptionRange(value);
            } else if (key == "hkeepaliveperiod"){
                obj["h_keep_alive_period"] = value.toInteger();
            }
        }
        return obj;
    }

    QJsonObject CoreObj_box::getXbadoptionRange(const QJsonValue & value){
        QJsonObject obj ;
        if (value.isString()){
            QString str = value.toString();
            auto ptr = str.split("-");
            obj.insert("from", ptr[0].toInt());
            obj.insert("to", ptr[(ptr.count()<2)?0:1].toInt());
        } else if (value.isObject()){
            auto objv = value.toObject();
            obj.insert("from", objv.value("from").toInt());
            obj.insert("to", objv.value("to").toInt());
        } else {
            obj.insert("from", value.toInt());
            obj.insert("to", value.toInt());
        }
        return obj;
    }

    void CoreObj_box::parseExtraXhttp(QJsonObject & transport, QString extra){
        extra = extra.replace("+", "");
        for (auto [k, v]: asKeyValueRange(QJsonDocument::fromJson(extra.toUtf8()).object().toVariantMap())){
            QString key = k.toLower().replace("_", "");
            QJsonValue value = v.toJsonValue();
            if (key == "xpaddingbytes"){
                transport["x_padding_bytes"] = getXbadoptionRange(value);
            } else if (key == "nogrpcheader"){
                transport["no_grpc_header"] = value.toBool();
            } else if (key == "nosseheader"){
                transport["no_sse_header"] = value.toBool();
            } else if (key == "scmaxeachpostbytes"){
                transport["sc_max_each_post_bytes"] = getXbadoptionRange(value);
            } else if (key == "scminpostsintervalms"){
                transport["sc_min_posts_interval_ms"] = getXbadoptionRange(value);
            } else if (key == "scmaxbufferedposts"){
                transport["sc_max_buffered_posts"] = value.toInteger();
            } else if (key == "scstreamupserversecs"){
                transport["sc_stream_up_server_secs"] = getXbadoptionRange(value);
            } else if (key == "domainstrategy"){
                transport["domain_strategy"] = value.toInt();
            } else if (key == "headers"){
                transport["headers"] = value.toObject();
            } else if (key == "xmux"){
                transport["xmux"] = getXmux(value);
            }
        }
    }

    using namespace CoreObj_box;

    void V2rayStreamSettings::BuildStreamSettingsSingBox(QJsonObject *outbound) {
        // https://sing-box.sagernet.org/configuration/shared/v2ray-transport
        QString type = outbound->value("type").toString();
        bool is_naive = type == "naive";
        if (is_naive) goto skip_network;
        if (network != "tcp") {
            QJsonObject transport{{"type", network}};
            if (network == "ws") {
                // ws path & ed
                auto pathWithoutEd = SubStrBefore(path, "?ed=");
                add_non_empty(transport, "path", pathWithoutEd);
                if (pathWithoutEd != path) {
                    if (auto ed = SubStrAfter(path, "?ed=").toInt(); ed > 0) {
                        transport["max_early_data"] = ed;
                        transport["early_data_header_name"] = "Sec-WebSocket-Protocol";
                    }
                }
                bool ok;
                auto headerMap = GetHeaderPairs(&ok);
                if (!ok) {
                    MW_show_log("Warning: headers could not be parsed, they will not be used");
                }
                add_non_empty(headerMap, "Host", host);
                transport["headers"] = QMapString2QJsonObject(headerMap);
                if (ws_early_data_length > 0) {
                    transport["max_early_data"] = ws_early_data_length;
                    transport["early_data_header_name"] = ws_early_data_name;
                }
            } else if (network == "http") {
                add_non_empty(transport, "path", path);
                add_non_empty(transport, "method", method.toUpper());
                if (!host.isEmpty()) transport["host"] = QListStr2QJsonArray(host.split(","));
                bool ok;
                auto headerMap = GetHeaderPairs(&ok);
                if (!ok) {
                    MW_show_log("Warning: headers could not be parsed, they will not be used");
                }
                transport["headers"] = QMapString2QJsonObject(headerMap);
            } else if (network == "grpc") {
                add_non_empty(transport, "service_name", path);
            } else if (network == "httpupgrade") {
                add_non_empty(transport, "path", path);
                add_non_empty(transport, "host", host);
                bool ok;
                auto headerMap = GetHeaderPairs(&ok);
                if (!ok) {
                    MW_show_log("Warning: headers could not be parsed, they will not be used");
                }
                transport["headers"] = QMapString2QJsonObject(headerMap);
            } else if (network == "xhttp") {
                add_non_empty(transport, "path", path);
                add_non_empty(transport, "host", host);
                transport["mode"] = xhttp_mode;
                parseExtraXhttp(transport, xhttp_extra);
            }
            if (!network.trimmed().isEmpty()) outbound->insert("transport", transport);
        } else if (header_type == "http") {
            // TCP + headerType
            QJsonObject transport{
                {"type", "http"},
                {"method", "GET"},
                {"path", path},
                {"headers", QJsonObject{{"Host", QListStr2QJsonArray(host.split(","))}}},
            };
            if (!network.trimmed().isEmpty()) outbound->insert("transport", transport);
        }
        skip_network:
        // tls
        if (security == "tls") {
            QJsonObject tls{{"enabled", true}};
            if (enable_ech){
                QJsonObject ech{
                    {"enabled", true},
                    {"config", QListStr2QJsonArray(ech_config.trimmed().split("\n"))}
                };
                add_non_empty(ech, "query_server_name", query_server_name);
                tls["ech"] = ech;
            }
            add_non_empty(tls, "server_name", sni);
            if (!certificate.trimmed().isEmpty()) {
                tls["certificate"] = certificate.trimmed();
            }
            if (!is_naive){
                if (allow_insecure || Configs::dataStore->skip_cert) tls["insecure"] = true;
                if (!alpn.trimmed().isEmpty()) {
                    tls["alpn"] = QListStr2QJsonArray(alpn.split(","));
                }
                QString fp = utlsFingerprint;
                if (!reality_pbk.trimmed().isEmpty()) {
                    tls["reality"] = QJsonObject{
                        {"enabled", true},
                        {"public_key", reality_pbk},
                        {"short_id", reality_sid.split(",")[0]},
                    };
                    if (fp.isEmpty()) fp = "random";
                }
                if (!fp.isEmpty()) {
                    tls["utls"] = QJsonObject{
                            {"enabled", true},
                            {"fingerprint", fp},
                        };
                }
                if (enable_tls_fragment)
                {
                    tls["fragment"] = enable_tls_fragment;
                    if (!tls_fragment_fallback_delay.isEmpty()) tls["fragment_fallback_delay"] = tls_fragment_fallback_delay;
                }
                if (enable_tls_record_fragment) tls["record_fragment"] = enable_tls_record_fragment;
            } 
            outbound->insert("tls", tls);
        }

        if (type == "vmess" || type == "vless") {
            outbound->insert("packet_encoding", packet_encoding);
        }
    }


} // namespace Configs
