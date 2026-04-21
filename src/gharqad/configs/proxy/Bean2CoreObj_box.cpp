#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    static QJsonObject getXbadoptionRange(const QJsonValue & value);


    template<typename T>
    static void add_default_fields(T & obj, const AbstractBean * bean){
        obj["type"] = bean->type();
        obj["server"] = bean->entity->serverAddress;
        obj["server_port"] = bean->entity->serverPort;
    }

    template<typename T>
    static void add_non_empty(T & obj, const QString & key, const QString & value){
        if (!value.isEmpty()){
            obj[key] = value;
        }
    }

    static QJsonValue udp_over_tcp_object(int version){
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

    template<typename T, typename B>
    static void add_username_password(T & obj, B * bean){
        add_non_empty(obj, "password", bean->password);
        add_non_empty(obj, "username", bean->username);
    }

    template<typename T, typename B>
    static void add_network(T & obj, B * bean){
        if (bean->network->value > 0){
            add_non_empty(obj, "network", *bean->network);
        }
    }

    template<typename T, typename B>
    static void add_udp_over_tcp(T & obj, B * bean){
        obj["udp_over_tcp"] = udp_over_tcp_object(bean->uot);
    }

    template<typename T, typename B>
    static void add_quic(T & obj, B * bean){
        bool quic = bean->quic;
        obj["quic"] = quic;
        if (quic){
            obj["quic_congestion_control"] = *bean->quic_congestion_control;
        }
    }

    template<typename T>
    static void add_non_empty(T & obj, const QString & key, const QVariantMap & value){
        if (!value.isEmpty()){
            obj[key] = QJsonObject::fromVariantMap(value);
        }
    }

    static QJsonObject getXmux(const QJsonValue & value){
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

    static QJsonObject getXbadoptionRange(const QJsonValue & value){
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

    static void parseExtraXhttp(QJsonObject & transport, QString extra){
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

CoreObjOutboundBuildResult SocksBean::BuildCoreObjSingBox() const {
    CoreObjOutboundBuildResult result;

    QJsonObject outbound;
    outbound["version"] = QString::number(socks_http_type);
    add_default_fields(outbound, this);
    add_username_password(outbound, this);
    add_udp_over_tcp(outbound, this);
    add_network(outbound, this);

    result.outbound = outbound;
    return result;
}

    CoreObjOutboundBuildResult HttpBean::BuildCoreObjSingBox() const {
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

    CoreObjOutboundBuildResult ShadowSocksBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

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

    CoreObjOutboundBuildResult AnyTLSBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

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


    CoreObjOutboundBuildResult ShadowTLSBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

        QJsonObject outbound{
            {"password", password},
            {"version", shadowtls_version},
        };

        add_default_fields(outbound, this);

        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

    CoreObjOutboundBuildResult VMessBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

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

    CoreObjOutboundBuildResult TrojanVLESSBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

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

    CoreObjOutboundBuildResult QUICBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

        QJsonObject coreTlsObj{
            {"enabled", true},
            {"disable_sni", disableSni},
            {"insecure", allowInsecure},
            {"certificate", caText.trimmed()},
            {"server_name", sni},
        };
        if (!alpn.trimmed().isEmpty()) coreTlsObj["alpn"] = QListStr2QJsonArray(alpn.split(","));
        if (proxy_type == proxy_Hysteria2) coreTlsObj["alpn"] = "h3";

        QJsonObject outbound{
            {"tls", coreTlsObj},
        };
        add_default_fields(outbound, this);
        add_network(outbound, this);

        if (proxy_type == proxy_Hysteria) {
            outbound["obfs"] = obfsPassword;
            outbound["disable_mtu_discovery"] = disableMtuDiscovery;
            outbound["recv_window"] = streamReceiveWindow;
            outbound["recv_window_conn"] = connectionReceiveWindow;
            outbound["up_mbps"] = uploadMbps;
            outbound["down_mbps"] = downloadMbps;
            if (!serverPorts.empty())
            {
                outbound.remove("server_port");
                QStringList modifiedPorts;
                for (const QString& port : serverPorts) {
                    if (port.contains(":")) {
                        modifiedPorts.append(port);
                    } else {
                        modifiedPorts.append(port + ":" + port);
                    }
                }
                outbound["server_ports"] = QListStr2QJsonArray(modifiedPorts);
                add_non_empty(outbound, "hop_interval", hop_interval);
            }

            if (authPayloadType == hysteria_auth_base64) outbound["auth"] = authPayload;
            if (authPayloadType == hysteria_auth_string) outbound["auth_str"] = authPayload;
        } else if (proxy_type == proxy_Hysteria2) {
            outbound["password"] = password;
            outbound["up_mbps"] = uploadMbps;
            outbound["down_mbps"] = downloadMbps;
            if (!serverPorts.empty())
            {
                outbound.remove("server_port");
                QStringList modifiedPorts;
                for (const QString& port : serverPorts) {
                    if (port.contains(":")) {
                        modifiedPorts.append(port);
                    } else {
                        modifiedPorts.append(port + ":" + port);
                    }
                }
                outbound["server_ports"] = QListStr2QJsonArray(modifiedPorts);
                add_non_empty(outbound, "hop_interval", hop_interval);
            }

            if (!obfsPassword.isEmpty()) {
                outbound["obfs"] = QJsonObject{
                    {"type", "salamander"},
                    {"password", obfsPassword},
                };
            }
        } else if (proxy_type == proxy_TUIC) {
            outbound["uuid"] = uuid;
            outbound["password"] = password;
            outbound["congestion_control"] = congestionControl;
            if (uos) {
                outbound["udp_over_stream"] = true;
            } else {
                outbound["udp_relay_mode"] = udpRelayMode;
            }
            outbound["zero_rtt_handshake"] = zeroRttHandshake;
            add_non_empty(outbound, "heartbeat", heartbeat.trimmed());
        }

        result.outbound = outbound;
        return result;
    }

    CoreObjOutboundBuildResult WireguardBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

        auto tun_name = "nekobox-wg";

        QJsonObject peer{
            {"address", entity->serverAddress},
            {"port", entity->serverPort},
            {"public_key", publicKey},
            {"pre_shared_key", preSharedKey},
            {"reserved", QListInt2QJsonArray(reserved)},
            {"allowed_ips", QListStr2QJsonArray({"0.0.0.0/0", "::/0"})},
            {"persistent_keepalive_interval", persistentKeepalive},
        };
        QJsonObject outbound{
            {"type", "wireguard"},
            {"name", tun_name},
            {"address", QListStr2QJsonArray(localAddress)},
            {"private_key", privateKey},
            {"peers", QJsonArray{peer}},
            {"mtu", MTU},
            {"system", useSystemInterface},
            {"workers", workerCount}
        };
        if (enable_amnezia)
        {
            outbound["junk_packet_count"] = junk_packet_count;
            outbound["junk_packet_min_size"] = junk_packet_min_size;
            outbound["junk_packet_max_size"] = junk_packet_max_size;
            outbound["init_packet_junk_size"] = init_packet_junk_size;
            outbound["response_packet_junk_size"] = response_packet_junk_size;
            outbound["init_packet_magic_header"] = init_packet_magic_header;
            outbound["response_packet_magic_header"] = response_packet_magic_header;
            outbound["underload_packet_magic_header"] = underload_packet_magic_header;
            outbound["transport_packet_magic_header"] = transport_packet_magic_header;
        }

        result.outbound = outbound;
        return result;
    }

    CoreObjOutboundBuildResult TailscaleBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QJsonObject outbound{
            {"type", this->type()},
            {"state_directory", state_directory},
            {"auth_key", auth_key},
            {"control_url", control_url},
            {"ephemeral", ephemeral},
            {"hostname", hostname},
            {"accept_routes", accept_routes},
            {"exit_node", exit_node},
            {"exit_node_allow_lan_access", exit_node_allow_lan_access},
            {"advertise_routes", QListStr2QJsonArray(advertise_routes)},
            {"advertise_exit_node", advertise_exit_node},
        };

        result.outbound = outbound;
        return result;
    }

    CoreObjOutboundBuildResult SSHBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

        QJsonObject outbound{
            {"type", "ssh"},
            {"server", entity->serverAddress},
            {"server_port", entity->serverPort},
            {"user", user},
            {"password", password},
        };
        add_non_empty(outbound, "private_key", privateKey);
        add_non_empty(outbound, "private_key_path", privateKeyPath);
        add_non_empty(outbound, "private_key_passphrase", privateKeyPass);
        if (!hostKey.isEmpty()) outbound["host_key"] = QListStr2QJsonArray(hostKey);
        if (!hostKeyAlgs.isEmpty()) outbound["host_key_algorithms"] = QListStr2QJsonArray(hostKeyAlgs);
        add_non_empty(outbound, "client_version", clientVersion);

        result.outbound = outbound;
        return result;
    }

    CoreObjOutboundBuildResult CustomBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;

        if (core == "internal") {
            result.outbound = QString2QJsonObject(config_simple);
        }

        return result;
    }

    CoreObjOutboundBuildResult ExtraCoreBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QJsonObject outbound{
            {"type", "socks"},
            {"server", socksAddress},
            {"server_port", socksPort},
        };
        result.outbound = outbound;
        return result;
    }
    
    CoreObjOutboundBuildResult MieruBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QJsonObject outbound {
            {"server_ports", QListStr2QJsonArray(this->serverPorts)},
            {"transport", QString(*this->network).toUpper()},
            {"multiplexing", *this->multiplexing},
        };
        add_username_password(outbound, this);
        add_default_fields(outbound, this);
        add_non_empty(outbound, "traffic_pattern", traffic_pattern);
        result.outbound = outbound;
        return result;
    }


    CoreObjOutboundBuildResult NaiveBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QJsonObject outbound {
            {"insecure_concurrency", this->insecure_concurrency},
            {"extra_headers", QJsonObject::fromVariantMap(this->extra_headers)},
        };
        add_default_fields(outbound, this);
        add_username_password(outbound, this);
        add_udp_over_tcp(outbound, this);
        add_quic(outbound, this);

        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }


    CoreObjOutboundBuildResult TrustTunnelBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        add_username_password(outbound, this);
        add_quic(outbound, this);
        outbound["health_check"] = this->health_check;
        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }


    CoreObjOutboundBuildResult JuicityBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        outbound["uuid"] = this->username;
        outbound["password"] = this->password;
        stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

    CoreObjOutboundBuildResult TorBean::BuildCoreObjSingBox() const
    {
        CoreObjOutboundBuildResult result;
        QString path = this->executable_path;
        if (!path.isEmpty() || !QFile::exists(path)){
            path = QStandardPaths::findExecutable("tor");
        }

        QJsonObject outbound {
            {"type", this->type()},
            {"executable_path", path},
            {"extra_args", QListStr2QJsonArray(this->extra_args)},
            {"data_directory", this->data_directory},
            {"torrc", QJsonObject::fromVariantMap(this->torrc)}
        };
        result.outbound = outbound;
        return result;
    }
} // namespace Configs
