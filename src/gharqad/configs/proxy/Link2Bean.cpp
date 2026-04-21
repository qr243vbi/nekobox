#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <3rdparty/URLParser/url_parser.h>

#include <QUrlQuery>

namespace Configs {

template<typename T>
static void add_username_password(T * t, QUrl & url){
    auto username = url.userName();
    auto password = url.password();
    // v2rayN fmt
    if (password.isEmpty() && !username.isEmpty()) {
        QString n = DecodeB64IfValid(username);
        if (!n.isEmpty()) {
            username = SubStrBefore(n, ":");
            password = SubStrAfter(n, ":");
        }
    }
    t->username = username;
    t->password = password;
}

template<typename T>
static void add_udp_over_tcp(T *t, QUrlQuery & query){
    t->uot = GetQueryIntValue(query, "uot", GetQueryIntValue(query, "udp_over_tcp", 0));
}

    template<typename T>
    static void add_quic(T * bean, const QUrlQuery &query){
        *bean->quic_congestion_control = GetQueryValue(query, "quic_congestion_control");
        bean->quic = (GetQueryValue(query, "quic").localeAwareCompare("true") == 0);
    }

    template<typename T>
    static void add_network(T * t, const QUrlQuery &obj){
        auto network =  GetQueryValue(obj, "network");
        if (network.isEmpty()){
            *t->network = network;
        }
    }

    static void set_boolean(const char * name, bool & value, const QUrlQuery &obj){
        auto qval = obj.queryItemValue(name);
        if (value){
            value = !(qval.localeAwareCompare("false"));
        } else {
            value = (qval.localeAwareCompare("true"));
        }
    }

    static void add_mux_state(AbstractBean * bean, const QUrlQuery &query){
        auto mux_str = GetQueryValue(query, "mux", "");
        if (mux_str == "true") {
            bean->mux_state = 1;
        } else if (mux_str == "false") {
            bean->mux_state = 2;
        } else {
            bean->mux_state = 0;
        }
    }

static void add_tls(std::shared_ptr<V2rayStreamSettings> stream, QUrlQuery & query){
    stream->security = "tls";
    auto sni1 = GetQueryValue(query, "sni");
    auto sni2 = GetQueryValue(query, "peer");
    if (!sni1.isEmpty()) stream->sni = sni1;
    if (!sni2.isEmpty()) stream->sni = sni2;
    stream->alpn = GetQueryValue(query, "alpn");
    stream->allow_insecure = !QStringList{"0", "false"}.contains(query.queryItemValue("insecure"));
    auto ech_config = GetQueryValue(query, "ech_config");
    if ((stream->enable_ech = (!ech_config.isEmpty()))){
        stream->ech_config = ech_config;
        stream->query_server_name = GetQueryValue(query, "query_server_name");
    }

    stream->reality_pbk = GetQueryValue(query, "pbk", "");
    stream->reality_sid = GetQueryValue(query, "sid", "");
    stream->utlsFingerprint = GetQueryValue(query, "fp", "");
    if (query.queryItemValue("fragment") == "1") stream->enable_tls_fragment = true;
    stream->tls_fragment_fallback_delay = query.queryItemValue("fragment_fallback_delay");
    if (query.queryItemValue("record_fragment") == "1") stream->enable_tls_record_fragment = true;
    if (stream->utlsFingerprint.isEmpty()) {
        stream->utlsFingerprint = dataStore->utlsFingerprint;
    }
}

    static void add_default_fields(QUrl & url, Configs::ProxyEntity * entity){
        entity->name = url.fragment(QUrl::FullyDecoded);
        entity->serverAddress = url.host();
        entity->serverPort = url.port();
    }

#define DECODE_V2RAY_N_1                                                                                                        \
    QString linkN = DecodeB64IfValid(SubStrBefore(SubStrAfter(link, "://"), "#"), QByteArray::Base64Option::Base64UrlEncoding); \
    if (linkN.isEmpty()) return false;                                                                                          \
    auto hasRemarks = link.contains("#");                                                                                       \
    if (hasRemarks) linkN += "#" + SubStrAfter(link, "#");                                                                      \
    auto url = QUrl("https://" + linkN);

    bool SocksBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        if (link.startsWith("socks4")) socks_http_type = type_Socks4;
        add_default_fields(url, entity);
        if (entity->serverPort == -1) entity->serverPort = 1080;

        add_username_password(this, url);
        add_network(this, query);
        add_udp_over_tcp(this, query);
        return !entity->serverAddress.isEmpty();
    }

    bool HttpBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        add_default_fields(url, entity);
        if (entity->serverPort == -1) entity->serverPort = 443;
        add_username_password(this, url);
        path = url.path();
        headers = GetQueryMapValue(query, "headers");
        if (link.startsWith("https")) {
            add_tls(stream, query);
        };
        return !entity->serverAddress.isEmpty();
    }

    bool AnyTLSBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        password = url.userName();
        if (entity->serverPort == -1) entity->serverPort = 443;
        this->idle_session_check_interval = GetQueryValue(query, "idle_session_check_interval", "30s");
        this->idle_session_timeout = GetQueryValue(query, "idle_session_timeout", "30s");
        this->min_idle_session = GetQueryIntValue(query, "min_idle_session", 0);
        // security
        add_tls(stream, query);

        return !(password.isEmpty() || entity->serverAddress.isEmpty());
    }

    bool ShadowTLSBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        password = url.userName();
        if (entity->serverPort == -1) entity->serverPort = 443;
        this->shadowtls_version = GetQueryIntValue(query, "version", 0);
        // security
        add_tls(stream, query);

        return !(password.isEmpty() || entity->serverAddress.isEmpty());
    }


    bool TrojanVLESSBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        password = url.userName();
        if (entity->serverPort == -1) entity->serverPort = 443;

        add_network(this, query);
        // security

        auto type =  GetQueryValue(query, "type", "tcp");
        if (type == "h2") {
            type = "http";
        }
        stream->network = type;

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
        if (stream->network == "ws") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "");
        } else if (stream->network == "http") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "").replace("|", ",");
            stream->method = GetQueryValue(query, "method", "");
        } else if (stream->network == "httpupgrade") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "");
        } else if (stream->network == "grpc") {
            stream->path = GetQueryValue(query, "serviceName", "");
        } else if (stream->network == "tcp") {
            if (GetQueryValue(query, "headerType") == "http") {
                stream->header_type = "http";
                stream->host = GetQueryValue(query, "host", "");
                stream->path = GetQueryValue(query, "path", "");
            }
        } else if (stream->network == "xhttp") {
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
            stream->packet_encoding = GetQueryValue(query, "packetEncoding", "xudp");
        }

        return !(password.isEmpty() || entity->serverAddress.isEmpty());
    }

    bool ShadowSocksBean::TryParseLink(const QString &link) {
        if (SubStrBefore(link, "#").contains("@")) {
            // SS
            auto url = QUrl(link);
            if (!url.isValid()) return false;
            add_default_fields(url, entity);

            if (url.password().isEmpty()) {
                // traditional format
                auto method_password = DecodeB64IfValid(url.userName(), QByteArray::Base64Option::Base64UrlEncoding);
                if (method_password.isEmpty()) return false;
                method = SubStrBefore(method_password, ":");
                password = SubStrAfter(method_password, ":");
            } else {
                // 2022 format
                method = url.userName();
                password = url.password();
            }

            auto query = GetQuery(url);
            plugin = query.queryItemValue("plugin").replace("simple-obfs;", "obfs-local;");
            
            add_mux_state(this, query);
            add_network(this, query);
            add_udp_over_tcp(this, query);

        } else {
            // v2rayN
            DECODE_V2RAY_N_1

            if (hasRemarks) entity->name = url.fragment(QUrl::FullyDecoded);
            entity->serverAddress = url.host();
            entity->serverPort = url.port();
            method = url.userName();
            password = url.password();
        }

        return !(entity->serverAddress.isEmpty() || method.isEmpty() || password.isEmpty());
    }

    bool VMessBean::TryParseLink(const QString &link) {
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
                stream->network = net;
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
            auto type = GetQueryValue(query, "type", "tcp");
            if (type == "h2") {
                type = "http";
            }
            stream->network = type;
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
            if (stream->network == "ws") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "");
            } else if (stream->network == "http") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "").replace("|", ",");
            } else if (stream->network == "httpupgrade") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "");
            } else if (stream->network == "grpc") {
                stream->path = GetQueryValue(query, "serviceName", "");
            } else if (stream->network == "tcp") {
                if (GetQueryValue(query, "headerType") == "http") {
                    stream->header_type = "http";
                    stream->path = GetQueryValue(query, "path", "");
                    stream->host = GetQueryValue(query, "host", "");
                }
            } else if (stream->network == "xhttp") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "");
                stream->xhttp_mode = GetQueryValue(query, "mode", "auto");
                stream->xhttp_extra = GetQueryValue(query, "extra", "");
            }
            return !(uuid.isEmpty() || entity->serverAddress.isEmpty());
        }

        return false;
    }

    bool QUICBean::TryParseLink(const QString &link) {


        auto url = QUrl(link);
        if (!url.isValid()) {
            if(!url.errorString().startsWith("Invalid port"))
                return false;
            entity->serverPort = 0;
            serverPorts = QString::fromStdString(URLParser::Parse((link.split("?")[0] + "/").toStdString()).port).split(",");
            for (int i=0; i < serverPorts.size(); i++) {
                serverPorts[i].replace("-", ":");
            }
        }
        auto query = QUrlQuery(url.query());

        add_network(this, query);
        if (url.scheme() == "hysteria") {
            // https://hysteria.network/docs/uri-scheme/
            if (!query.hasQueryItem("upmbps") || !query.hasQueryItem("downmbps")) return false;

            entity->name = url.fragment(QUrl::FullyDecoded);
            entity->serverAddress = url.host();
            if (entity->serverPort > 0) entity->serverPort = url.port();
            obfsPassword = QUrl::fromPercentEncoding(query.queryItemValue("obfsParam").toUtf8());
            allowInsecure = QStringList{"1", "true"}.contains(query.queryItemValue("insecure"));
            uploadMbps = GetQueryIntValue(query, "upmbps");
            downloadMbps = GetQueryIntValue(query, "downmbps");

            auto protocolStr = (query.hasQueryItem("protocol") ? query.queryItemValue("protocol") : "udp").toLower();
            if (protocolStr == "faketcp") {
                hyProtocol = Configs::QUICBean::hysteria_protocol_facktcp;
            } else if (protocolStr.startsWith("wechat")) {
                hyProtocol = Configs::QUICBean::hysteria_protocol_wechat_video;
            }

            if (query.hasQueryItem("auth")) {
                authPayload = query.queryItemValue("auth");
                authPayloadType = Configs::QUICBean::hysteria_auth_string;
            }

            alpn = query.queryItemValue("alpn");
            sni = FIRST_OR_SECOND(query.queryItemValue("peer"), query.queryItemValue("sni"));

            connectionReceiveWindow = GetQueryIntValue(query, "recv_window");
            streamReceiveWindow = GetQueryIntValue(query, "recv_window_conn");

            if (query.hasQueryItem("mport")) {
                serverPorts = query.queryItemValue("mport").split(",");
                for (int i=0; i < serverPorts.size(); i++) {
                    serverPorts[i].replace("-", ":");
                }
            }
            hop_interval = query.queryItemValue("hop_interval");
        } else if (url.scheme() == "tuic") {
            // by daeuniverse
            // https://github.com/daeuniverse/dae/discussions/182
            add_default_fields(url, entity);

            if (entity->serverPort == -1) entity->serverPort = 443;

            uuid = url.userName();
            password = url.password();

            congestionControl = query.queryItemValue("congestion_control");
            alpn = query.queryItemValue("alpn");
            sni = query.queryItemValue("sni");
            udpRelayMode = query.queryItemValue("udp_relay_mode");
            allowInsecure = query.queryItemValue("allow_insecure") == "1";
            disableSni = query.queryItemValue("disable_sni") == "1";
        } else if (QStringList{"hy2", "hysteria2"}.contains(url.scheme())) {
            entity->name = url.fragment(QUrl::FullyDecoded);
            entity->serverAddress = url.host();
            if (entity->serverPort > 0) entity->serverPort = url.port();
            obfsPassword = QUrl::fromPercentEncoding(query.queryItemValue("obfs-password").toUtf8());
            allowInsecure = QStringList{"1", "true"}.contains(query.queryItemValue("insecure"));

            if (url.password().isEmpty()) {
                password = url.userName();
            } else {
                password = url.userName() + ":" + url.password();
            }
            if (query.hasQueryItem("mport")) {
                serverPorts = query.queryItemValue("mport").split(",");
                for (int i=0; i < serverPorts.size(); i++) {
                    serverPorts[i].replace("-", ":");
                }
            }
            hop_interval = query.queryItemValue("hop_interval");

            sni = query.queryItemValue("sni");
        }

        return true;
    }

    bool WireguardBean::TryParseLink(const QString &link) {
        if (parseWgConfig(link)) return true;

        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        privateKey = query.queryItemValue("private_key");
        publicKey = query.queryItemValue("peer_public_key");
        preSharedKey = query.queryItemValue("pre_shared_key");
        auto rawReserved = query.queryItemValue("reserved");
        if (!rawReserved.isEmpty()) {
            for (const auto &item: rawReserved.split("-")) reserved += item.toInt();
        }
        auto rawLocalAddr = query.queryItemValue("local_address");
        if (!rawLocalAddr.isEmpty()) {
            for (const auto &item: rawLocalAddr.split("-")) localAddress += item;
        }
        persistentKeepalive = GetQueryIntValue(query, "persistent_keepalive");
        MTU = GetQueryIntValue(query, "mtu");
        set_boolean("use_system_interface", useSystemInterface, query);
        workerCount = GetQueryIntValue(query, "workers");

        set_boolean("enable_amnezia", enable_amnezia, query);
        junk_packet_count = GetQueryIntValue(query, "junk_packet_count");
        junk_packet_min_size = GetQueryIntValue(query, "junk_packet_min_size");
        junk_packet_max_size = GetQueryIntValue(query, "junk_packet_max_size");
        init_packet_junk_size = GetQueryIntValue(query, "init_packet_junk_size");
        response_packet_junk_size = GetQueryIntValue(query, "response_packet_junk_size");
        init_packet_magic_header = GetQueryIntValue(query, "init_packet_magic_header");
        response_packet_magic_header = GetQueryIntValue(query, "response_packet_magic_header");
        underload_packet_magic_header = GetQueryIntValue(query, "underload_packet_magic_header");
        transport_packet_magic_header = GetQueryIntValue(query, "transport_packet_magic_header");

        return true;
    }

    bool TailscaleBean::TryParseLink(const QString &link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        entity->name = url.fragment(QUrl::FullyDecoded);

        state_directory = QUrl::fromPercentEncoding(query.queryItemValue("state_directory").toUtf8());
        auth_key = QUrl::fromPercentEncoding(query.queryItemValue("auth_key").toUtf8());
        control_url = QUrl::fromPercentEncoding(query.queryItemValue("control_url").toUtf8());
        set_boolean("ephemeral", ephemeral, query);
        hostname = QUrl::fromPercentEncoding(query.queryItemValue("hostname").toUtf8());
        set_boolean("accept_routes", accept_routes, query);
        exit_node = query.queryItemValue("exit_node");
        set_boolean("exit_node_allow_lan_access", exit_node_allow_lan_access, query);
        advertise_routes = QUrl::fromPercentEncoding(query.queryItemValue("advertise_routes").toUtf8()).split(",");
        set_boolean("advertise_exit_node", advertise_exit_node, query);
        set_boolean("globalDNS", globalDNS, query);
        set_boolean("global_dns", globalDNS, query);

        return true;
    }

    bool SSHBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        user = query.queryItemValue("user");
        password = query.queryItemValue("password");
        privateKey = QByteArray::fromBase64(query.queryItemValue("private_key").toUtf8(), QByteArray::OmitTrailingEquals);
        privateKeyPath = query.queryItemValue("private_key_path");
        privateKeyPass = query.queryItemValue("private_key_passphrase");
        auto hostKeysRaw = query.queryItemValue("host_key");
        for (const auto &item: hostKeysRaw.split("-")) {
            auto b64hostKey = QByteArray::fromBase64(item.toUtf8(), QByteArray::OmitTrailingEquals);
            if (!b64hostKey.isEmpty()) hostKey << QString(b64hostKey);
        }
        auto hostKeyAlgsRaw = query.queryItemValue("host_key_algorithms");
        for (const auto &item: hostKeyAlgsRaw.split("-")) {
            auto b64hostKeyAlg = QByteArray::fromBase64(item.toUtf8(), QByteArray::OmitTrailingEquals);
            if (!b64hostKeyAlg.isEmpty()) hostKeyAlgs << QString(b64hostKeyAlg);
        }
        clientVersion = query.queryItemValue("client_version");

        return true;
    }

    bool ExtraCoreBean::TryParseLink(const QString& link)
    {
        return false;
    }

    bool TorBean::TryParseLink(const QString &link){
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto q = GetQuery(url);
        entity->name = url.fragment(QUrl::FullyDecoded);

        extra_args = GetQueryListValue(q, "extra_args");
        executable_path = GetQueryValue(q, "executable_path");
        data_directory = GetQueryValue(q, "data_directory");
        torrc = GetQueryMapValue(q, "torrc");
        return true;
    };

    bool NaiveBean::TryParseLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        QUrlQuery query = GetQuery(url);
        add_default_fields(url, entity);

        add_username_password(this, url);
        add_quic(this, query);
        add_udp_over_tcp(this, query);
        extra_headers = GetQueryMapValue(query, "extra_headers");

        add_tls(stream, query);
        return true;
    }


    bool TrustTunnelBean::TryParseLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        QUrlQuery query = GetQuery(url);
        add_default_fields(url, entity);

        add_username_password(this, url);
        add_quic(this, query);
        set_boolean("health_check", this->health_check, query);

        add_tls(stream, query);
        return true;
    }


    bool JuicityBean::TryParseLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        QUrlQuery query = GetQuery(url);
        add_default_fields(url, entity);
        add_username_password(this, url);
        add_tls(stream, query);
        return true;
    }

    bool MieruBean::TryParseLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        add_username_password(this, url);

        *network = query.queryItemValue("transport").toLower();
        traffic_pattern = GetQueryValue(query, "traffic_pattern");
        *multiplexing = query.queryItemValue("multiplexing");
        serverPorts = query.queryItemValue("server_ports").split(",");
        return true;
    }
} // namespace Configs
