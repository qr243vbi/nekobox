#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
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

    int CoreObj_box::domainStrategy(const QJsonValue &value){
        if (value.isDouble()){
            return value.toInt();
        } else {
            QString strategy = value.toString().toLower().replace("_", "");
            if (strategy == "preferipv4"){
                return 1;
            } else if (strategy == "preferipv6"){
                return 2;
            } else if (strategy == "ipv4only"){
                return 3;
            } else if (strategy == "ipv6only"){
                return 4;
            } else {
                return 0;
            }
        }
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

    void CoreObj_box::parseExtraKCP(QJsonObject & transport, std::shared_ptr<KCPExtra> extra){
        if (extra->mtu >= 0) { transport["mtu"] = extra->mtu; }
        if (extra->tti >= 0) { transport["tti"] = extra->tti; }
        if (extra->uplinkcapacity >= 0) { transport["uplink_capacity"] = extra->uplinkcapacity; }
        if (extra->downlinkcapacity >= 0) { transport["downlink_capacity"] = extra->downlinkcapacity; }
        if (extra->congestion) { transport["congestion"] = true; }
        if (extra->readbuffersize >= 0) { transport["read_buffer_size"] = extra->readbuffersize; }
        if (extra->writebuffersize >= 0) { transport["write_buffer_size"] = extra->writebuffersize; }
        if (!extra->headertype.isEmpty()) { transport["header_type"] = extra->headertype; }
        if (!extra->seed.isEmpty()) { transport["seed"] = extra->seed; }
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
                transport["domain_strategy"] = domainStrategy(value);
            } else if (key == "headers"){
                transport["headers"] = value.toObject();
            } else if (key == "xmux"){
                transport["xmux"] = getXmux(value);
            }
            // new options
            else if (key == "xpaddingobfsmode"){
                transport["x_padding_obfs_mode"] = value.toBool();
            } else if (key == "xpaddingkey"){
                transport["x_padding_key"] = value.toString();
            } else if (key == "xpaddingheader"){
                transport["x_padding_header"] = value.toString();
            } else if (key == "xpaddingplacement"){
                transport["x_padding_placement"] = value.toString();
            } else if (key == "xpaddingmethod"){
                transport["x_padding_method"] = value.toString();
            } else if (key == "uplinkhttpmethod"){
                transport["uplink_http_method"] = value.toString();
            } else if (key == "sessionplacement"){
                transport["session_placement"] = value.toString();
            } else if (key == "sessionkey"){
                transport["session_key"] = value.toString();
            } else if (key == "seqplacement"){
                transport["seq_placement"] = value.toString();
            } else if (key == "seqkey"){
                transport["seq_key"] = value.toString();
            } else if (key == "uplinkdataplacement"){
                transport["uplink_data_placement"] = value.toString();
            } else if (key == "uplinkdatakey"){
                transport["uplink_data_key"] = value.toString();
            } else if (key == "uplinkchunksize"){
                transport["uplink_chunk_size"] = value.toString();
            } else if (key == "server"){
                transport["server"] = value.toString();
            } else if (key == "serverport"){
                transport["server_port"] = value.toInt();
            } else if (key == "detour"){
                transport["detour"] = value.toString();
            } else if (key == "tls"){
                transport["tls"] = value.toObject();
            }
        }
    }

    using namespace CoreObj_box;

    void V2rayStreamSettings::BuildStreamSettingsSingBox(QJsonObject *outbound) {
        // https://sing-box.sagernet.org/configuration/shared/v2ray-transport
        QString type = outbound->value("type").toString();
        bool is_naive = type == "naive";
        QString network = (QString) *this->network;
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
            } else if (network == "kcp") {
                parseExtraKCP(transport, kcp_extra);
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
            outbound->insert("packet_encoding", (QString)*packet_encoding);
        }
    }


} // namespace Configs
