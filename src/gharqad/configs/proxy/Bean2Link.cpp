#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <QUrlQuery>
#include <QProcess>
#include <qurlquery.h>

namespace Configs {
    inline void add_query_int_natural(const char * name, QUrlQuery & query, int value){ AddQueryNatural(query, name, value); };
    inline void add_query_int(const char * name, QUrlQuery & query, int value){ AddQueryInt(query, name, value); };
    inline void add_query_nonempty(const char * name, QUrlQuery & query, const QString &value){ AddQueryString(query, name, value); };
    inline void add_query_args_nonempty(const char * name, QUrlQuery & query, const QStringList & value) { AddQueryStringList(query, name, value); };
    inline void add_query_map_nonempty(const char * name, QUrlQuery & query, const QVariantMap & value) { AddQueryMap(query, name, value); };
    
    template<typename T>
    static void add_network(QUrlQuery & query, T * obj){
        if (obj->network->value > 0){
            add_query_nonempty("network", query, *obj->network);
        }
    }

    static void add_default_fields(QUrl & url, const AbstractBean * bean){
        auto name = bean->entity->name;
        if (!name.isEmpty()) url.setFragment(name);
        url.setHost(bean->entity->serverAddress);
        url.setPort(bean->entity->serverPort);
    }

    inline void add_query_boolean(const char * name, QUrlQuery & query, bool value){
        add_query_nonempty(name, query, value ? "true" : "false");
    };

    template<typename B>
    static void add_quic(QUrlQuery & q, B * bean){
        add_query_nonempty("quic_congestion_control", q, (QString)*bean->quic_congestion_control);
        add_query_boolean("quic", q, bean->quic);
    }

    static void add_mux_state(QUrlQuery & q, const AbstractBean * bean){
        if (bean->mux_state != 0) {
            add_query_boolean("mux", q, (bean->mux_state == 1) ? true : false);
        }
    }

    static void add_query_int_range(const char * name, QUrlQuery & query, int value, int begin, int end){
        if (value >= begin && value <= end){
            add_query_int(name, query, value);
        }
    }

    template<typename T>
    static void add_udp_over_tcp(QUrlQuery & query, T * bean){
        add_query_int_range("uot", query, bean->uot, 1, 2);
    }


    template<typename B>
    static void add_username_password(QUrl & url, B * bean){
        if (!bean->username.isEmpty()) url.setUserName(bean->username);
        if (!bean->password.isEmpty()) url.setPassword(bean->password);
    }


    static void add_tls(std::shared_ptr<V2rayStreamSettings> stream, QUrlQuery & query){
        auto security = stream->security;
        if (security == "tls" && !stream->reality_pbk.trimmed().isEmpty()) security = "reality";
        query.addQueryItem("security", security == "" ? "none" : security);

        add_query_nonempty("sni", query, stream->sni);
        add_query_nonempty("alpn", query, stream->alpn);
        if (stream->enable_ech){
            add_query_nonempty("ech_config", query, stream->ech_config);
            add_query_nonempty("query_server_name", query, stream->query_server_name);
        }
        if (stream->allow_insecure) query.addQueryItem("insecure", "1");
        if (stream->utlsFingerprint.isEmpty()) {
            query.addQueryItem("fp", Configs::dataStore->utlsFingerprint);
        } else {
            query.addQueryItem("fp", stream->utlsFingerprint);
        }
        if (stream->enable_tls_fragment) query.addQueryItem("fragment", "1");
        add_query_nonempty("fragment_fallback_delay", query, stream->tls_fragment_fallback_delay);
        if (stream->enable_tls_record_fragment) query.addQueryItem("record_fragment", "1");

        if (security == "reality") {
            add_query_nonempty("pbk", query, stream->reality_pbk);
            add_query_nonempty("sid", query, stream->reality_sid);
        }

        // type
        add_query_nonempty("type", query, stream->network);

        if (stream->network == "ws" || stream->network == "httpupgrade") {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
        } else if (stream->network == "http" ) {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
            add_query_nonempty("method", query, stream->method);
        } else if (stream->network == "grpc") {
            add_query_nonempty("serviceName", query, stream->path);
        } else if (stream->network == "tcp") {
            if (stream->header_type == "http") {
                add_query_nonempty("path", query, stream->path);
                add_query_nonempty("headerType", query, "http");
                add_query_nonempty("host", query, stream->host);
            }
        } else if (stream->network == "xhttp") {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
            add_query_nonempty("mode", query, stream->xhttp_mode);
            add_query_nonempty("extra", query, stream->xhttp_extra);
        }

    }

    QString HttpBean::ToShareLink() const {
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

    QString SocksBean::ToShareLink() const {
        QUrl url;
        QUrlQuery query;
        add_default_fields(url, this);
        {
            url.setScheme(QString("socks%1").arg(socks_http_type));
        }
        add_username_password(url, this);
        add_network(query, this);
        add_udp_over_tcp(query, this);
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QString ShadowTLSBean::ToShareLink() const {
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

    QString AnyTLSBean::ToShareLink() const {
        QUrl url;
        QUrlQuery query;
        url.setScheme("anytls");
        url.setUserName(password);        
        add_default_fields(url, this);
        add_query_nonempty("idle_session_check_interval", query, idle_session_check_interval);
        add_query_nonempty("idle_session_timeout", query, idle_session_timeout);
        add_query_int_natural("min_idle_session", query, min_idle_session);

        //  security
        add_tls(stream, query);

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QString TrojanVLESSBean::ToShareLink() const {
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
            add_query_nonempty("packetEncoding", query, stream->packet_encoding);
            query.addQueryItem("encryption", (encryption == "" ) ? "none" : encryption);
        }

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    const char* fixShadowsocksUserNameEncodeMagic = "fixShadowsocksUserNameEncodeMagic-holder-for-QUrl";

    QString ShadowSocksBean::ToShareLink() const {
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

    QString VMessBean::ToShareLink() const {
        QUrl url;
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

    QString QUICBean::ToShareLink() const {
        QUrl url;
        QString portRange;
        QUrlQuery query;
        if (proxy_type == proxy_Hysteria) {
            url.setScheme("hysteria");
            url.setHost(entity->serverAddress);
            url.setPort(0);
            add_query_int("upmbps", query, uploadMbps);
            add_query_int("downmbps", query, downloadMbps);
            if (!obfsPassword.isEmpty()) {
                query.addQueryItem("obfs", "xplus");
                query.addQueryItem("obfsParam", QUrl::toPercentEncoding(obfsPassword));
            }
            if (authPayloadType == hysteria_auth_string) query.addQueryItem("auth", authPayload);
            if (hyProtocol == hysteria_protocol_facktcp) query.addQueryItem("protocol", "faketcp");
            if (hyProtocol == hysteria_protocol_wechat_video) query.addQueryItem("protocol", "wechat-video");
            if (allowInsecure) query.addQueryItem("insecure", "1");
            add_query_nonempty("peer", query, sni);
            add_query_nonempty("alpn", query, alpn);
            add_query_int_natural("recv_window", query, (connectionReceiveWindow));
            add_query_int_natural("recv_window_conn", query, (streamReceiveWindow));
            if (!serverPorts.empty()) {
                QStringList portList;
                for (const auto& range : serverPorts) {
                    QString modifiedRange = range;
                    modifiedRange.replace(":", "-");
                    portList.append(modifiedRange);
                }
                portRange = portList.join(",");
            } else
                url.setPort(entity->serverPort);
            if (!hop_interval.isEmpty()) query.addQueryItem("hop_interval", hop_interval);
            if (!entity->name.isEmpty()) url.setFragment(entity->name);
        } else if (proxy_type == proxy_TUIC) {
            url.setScheme("tuic");
            url.setUserName(uuid);
            url.setPassword(password);
            add_default_fields(url, this);

            if (!congestionControl.isEmpty()) query.addQueryItem("congestion_control", congestionControl);
            if (!alpn.isEmpty()) query.addQueryItem("alpn", alpn);
            if (!sni.isEmpty()) query.addQueryItem("sni", sni);
            if (!udpRelayMode.isEmpty()) query.addQueryItem("udp_relay_mode", udpRelayMode);
            if (allowInsecure) query.addQueryItem("allow_insecure", "1");
            if (disableSni) query.addQueryItem("disable_sni", "1");
        } else if (proxy_type == proxy_Hysteria2) {
            url.setScheme("hy2");
            url.setHost(entity->serverAddress);
            url.setPort(0);
            if (password.contains(":")) {
                url.setUserName(SubStrBefore(password, ":"));
                url.setPassword(SubStrAfter(password, ":"));
            } else {
                url.setUserName(password);
            }
            if (!obfsPassword.isEmpty()) {
                query.addQueryItem("obfs", "salamander");
                query.addQueryItem("obfs-password", QUrl::toPercentEncoding(obfsPassword));
            }
            if (allowInsecure) query.addQueryItem("insecure", "1");
            if (!sni.isEmpty()) query.addQueryItem("sni", sni);
            if (!serverPorts.empty()) {
                QStringList portList;
                for (const auto& range : serverPorts) {
                    QString modifiedRange = range;
                    modifiedRange.replace(":", "-");
                    portList.append(modifiedRange);
                }
                portRange = portList.join(",");
            } else
                url.setPort(entity->serverPort);
            if (!hop_interval.isEmpty()) query.addQueryItem("hop_interval", hop_interval);
            if (!entity->name.isEmpty()) url.setFragment(entity->name);
        }
        add_network(query, this);
        if (!query.isEmpty()) url.setQuery(query);

        if (portRange.isEmpty())
            return url.toString(QUrl::FullyEncoded);
        else
            return url.toString(QUrl::FullyEncoded).replace(":0?", ":" + portRange + "?");
    }

    QString WireguardBean::ToShareLink() const {
        QUrl url;
        url.setScheme("wg");
        add_default_fields(url, this);
        QUrlQuery query;
        add_query_nonempty("private_key", query, privateKey);
        add_query_nonempty("peer_public_key", query, publicKey);
        add_query_nonempty("pre_shared_key", query, preSharedKey);
        add_query_nonempty("reserved", query, FormatReserved());
        add_query_int("persistent_keepalive", query, (persistentKeepalive));
        add_query_int("mtu", query, (MTU));
        add_query_boolean("use_system_interface", query, useSystemInterface);
        add_query_nonempty("local_address", query, localAddress.join("-"));
        add_query_int("workers", query, (workerCount));
        if (enable_amnezia)
        {
            query.addQueryItem("enable_amnezia", "true");
            add_query_int("junk_packet_count", query, (junk_packet_count));
            add_query_int("junk_packet_min_size", query, (junk_packet_min_size));
            add_query_int("junk_packet_max_size", query, (junk_packet_max_size));
            add_query_int("init_packet_junk_size", query, (init_packet_junk_size));
            add_query_int("response_packet_junk_size", query, (response_packet_junk_size));
            add_query_int("init_packet_magic_header", query, (init_packet_magic_header));
            add_query_int("response_packet_magic_header", query, (response_packet_magic_header));
            add_query_int("underload_packet_magic_header", query, (underload_packet_magic_header));
            add_query_int("transport_packet_magic_header", query, (transport_packet_magic_header));
        }
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QString TailscaleBean::ToShareLink() const
    {
        QUrl url;
        url.setScheme("ts");
        url.setHost("tailscale");
        if (!entity->name.isEmpty()) url.setFragment(entity->name);
        QUrlQuery q;
        add_query_nonempty("state_directory", q, QUrl::toPercentEncoding(state_directory));
        add_query_nonempty("auth_key", q, QUrl::toPercentEncoding(auth_key));
        add_query_nonempty("control_url", q, QUrl::toPercentEncoding(control_url));
        add_query_boolean("ephemeral", q, ephemeral);
        add_query_nonempty("hostname", q, QUrl::toPercentEncoding(hostname));
        add_query_boolean("accept_routes", q, accept_routes);
        add_query_nonempty("exit_node", q, exit_node);
        add_query_boolean("exit_node_allow_lan_access", q, exit_node_allow_lan_access);
        add_query_nonempty("advertise_routes", q, QUrl::toPercentEncoding(advertise_routes.join(",")));
        add_query_boolean("advertise_exit_node", q, advertise_exit_node);
        add_query_boolean("global_dns", q, globalDNS);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString SSHBean::ToShareLink() const {
        QUrl url;
        url.setScheme("ssh");
        add_default_fields(url, this);
        QUrlQuery q;
        add_query_nonempty("user", q, user);
        add_query_nonempty("password", q, password);
        add_query_nonempty("private_key", q, privateKey.toUtf8().toBase64(QByteArray::OmitTrailingEquals));
        add_query_nonempty("private_key_path", q, privateKeyPath);
        add_query_nonempty("private_key_passphrase", q, privateKeyPass);
        QStringList b64HostKeys = {};
        for (const auto& item: hostKey) {
            b64HostKeys << item.toUtf8().toBase64(QByteArray::OmitTrailingEquals);
        }
        add_query_nonempty("host_key", q, b64HostKeys.join("-"));
        QStringList b64HostKeyAlgs = {};
        for (const auto& item: hostKeyAlgs) {
            b64HostKeyAlgs << item.toUtf8().toBase64(QByteArray::OmitTrailingEquals);
        }
        add_query_nonempty("host_key_algorithms", q, b64HostKeyAlgs.join("-"));
        add_query_nonempty("client_version", q, clientVersion);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString ExtraCoreBean::ToShareLink() const
    {
        return "Unsupported for now";
    }


    QString NaiveBean::ToShareLink() const {
        QUrl url;
        url.setScheme("naive");
        add_default_fields(url, this);
        QUrlQuery q;
        add_username_password(url, this);
        add_quic(q, this);
        add_udp_over_tcp(q, this);
        add_query_map_nonempty("extra_headers", q, extra_headers);
        add_tls(stream, q);

        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString TrustTunnelBean::ToShareLink() const {
        QUrl url;
        url.setScheme("tt");
        add_default_fields(url, this);
        QUrlQuery q;
        add_username_password(url, this);
        add_quic(q, this);
        add_tls(stream, q);
        add_query_boolean("health_check", q, health_check);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString JuicityBean::ToShareLink() const {
        QUrl url;
        url.setScheme("juicity");
        add_default_fields(url, this);
        QUrlQuery q;
        add_username_password(url, this);
        add_tls(stream, q);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString TorBean::ToShareLink() const {
        QUrl url;
        url.setScheme("tor");
        url.setHost("tor");
        QUrlQuery q;
        add_query_args_nonempty( "extra_args", q, extra_args);
        add_query_nonempty("executable_path", q, executable_path);
        add_query_nonempty("data_directory", q, data_directory);
        add_query_map_nonempty("torrc", q, torrc);

        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString MieruBean::ToShareLink() const {
        QUrl url;
        url.setScheme("mierus");
        add_default_fields(url, this);
        QUrlQuery q;
        add_username_password(url, this);
        add_query_nonempty( "transport", q, QString(*network).toUpper());
        add_query_nonempty( "multiplexing", q, *multiplexing);
        add_query_nonempty( "server_ports", q, serverPorts.join(","));
        add_query_nonempty("traffic_pattern", q, traffic_pattern);
        url.setQuery(q);

        return url.toString(QUrl::FullyEncoded);
    }

} // namespace Configs
