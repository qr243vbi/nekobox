#include "nekobox/configs/proxy/V2RayStreamSettings.hpp"
#include <nekobox/configs/proxy/includes.h>

namespace Configs
{

    static bool parse_tls(std::shared_ptr<V2rayStreamSettings> stream, const QJsonObject & obj){
        bool is_tls = obj["tls"].isObject() ;
        if (is_tls) {
            QJsonObject tls = obj["tls"].toObject();
            QJsonObject reality = tls["reality"].toObject();
            auto alpn = tls["alpn"];
            auto ech_config = tls["ech_config"];
            if (!ech_config.isNull()){
                stream->enable_ech = true;
                stream->ech_config = ech_config.isArray() ? QJsonArray2QListStr(ech_config.toArray()).join("\n") : ech_config.toString();
                stream->query_server_name = tls["query_server_name"].toString();
            }
            stream->security = "tls";
            stream->reality_pbk = reality["public_key"].toString();
            stream->reality_sid = reality["short_id"].toString();
            stream->utlsFingerprint = tls["utls"].toObject()["fingerprint"].toString();
            stream->enable_tls_fragment = tls["fragment"].toBool();
            stream->tls_fragment_fallback_delay = tls["fragment_fallback_delay"].toString();
            stream->enable_tls_record_fragment = tls["record_fragment"].toBool();
            stream->sni = tls["server_name"].toString();
            stream->alpn = alpn.isArray() ? QJsonArray2QListStr(alpn.toArray()).join(",") : alpn.toString();
            stream->allow_insecure = tls["insecure"].toBool();
        } else {
            stream->security = "";
        }
        return true;
    }

    static bool parse_transport(std::shared_ptr<V2rayStreamSettings> stream, const QJsonObject & obj){
        auto transport = obj["transport"].toObject();
        stream->network = transport["type"].toString();
        if (stream->network == "ws" || stream->network == "httpupgrade")
        {
            finalize:
            stream->path = transport["path"].toString();
            finalize2:
            auto host = transport["host"];
            stream->host = host.isArray() ? QJsonArray2QListStr(host.toArray()).join(",") : host.toString();
            return true;
        } else if (stream->network == "http")
        {
            stream->method = transport["method"].toString();
            goto finalize;
        } else if (stream->network == "grpc")
        {
            stream->path = transport["service_name"].toString();
            goto finalize2;
        } else if (stream->network == "xhttp")
        {
            stream->xhttp_mode = transport["mode"].toString();
            goto finalize;
        }
        return false;
    }

    bool QUICBean::TryParseJson(const QJsonObject& obj)
    {
        auto type = obj["type"].toString();
        if (type == "hysteria")
        {
            proxy_type = proxy_Hysteria;
            serverPorts = obj["server_ports"].isArray() ? QJsonArray2QListStr(obj["server_ports"].toArray()) : QStringList();
            hop_interval = obj["hop_interval"].toString();
            uploadMbps = obj["up_mbps"].isDouble() ? obj["up_mbps"].toInt() : 0;
            downloadMbps = obj["down_mbps"].isDouble() ? obj["down_mbps"].toInt() : 0;
            obfsPassword = obj["obfs"].toString();
            authPayloadType = obj["auth"].isString() ? hysteria_auth_base64 : hysteria_auth_string;
            authPayload = obj["auth"].isString() ? obj["auth"].toString() : obj["auth_str"].toString();
            disableMtuDiscovery = obj["disable_mtu_discovery"].toBool();
            connectionReceiveWindow = obj["recv_window_conn"].toInt();
            streamReceiveWindow = obj["recv_window"].toInt();

            goto finalize;
        }
        if (type == "hysteria2")
        {
            proxy_type = proxy_Hysteria2;
            serverPorts = obj["server_ports"].isArray() ? QJsonArray2QListStr(obj["server_ports"].toArray()) : QStringList();
            hop_interval = obj["hop_interval"].toString();
            uploadMbps = obj["up_mbps"].isDouble() ? obj["up_mbps"].toInt() : 0;
            downloadMbps = obj["down_mbps"].isDouble() ? obj["down_mbps"].toInt() : 0;
            obfsPassword = obj["obfs"].toObject()["password"].toString();
            password = obj["password"].toString();


            goto finalize;
        }
        if (type == "tuic")
        {
            proxy_type = proxy_TUIC;
            uuid = obj["uuid"].toString();
            congestionControl = obj["congestion_control"].toString();
            udpRelayMode = obj["udp_relay_mode"].toString();
            uos = obj["udp_over_stream"].toBool();
            zeroRttHandshake = obj["zero_rtt_handshake"].toBool();
            heartbeat = obj["heartbeat"].toString();
            password = obj["password"].toString();

            finalize:
            name = obj["tag"].toString();
            serverAddress = obj["server"].toString();
            serverPort = obj["server_port"].toInt();
            alpn = obj["tls"].toObject()["alpn"].isArray() ? QJsonArray2QListStr(obj["tls"].toObject()["alpn"].toArray()).join(",") : obj["tls"].toObject()["alpn"].toString();
            sni = obj["tls"].toObject()["server_name"].toString();
            disableSni = obj["tls"].toObject()["disable_sni"].toBool();
            allowInsecure = obj["tls"].toObject()["insecure"].toBool();
            return true;
        }
        return false;
    }

    bool ShadowSocksBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
//        method = obj["method"].toString();
//        password = obj["password"].toString();
//        plugin = obj["plugin"].toString();
        uot = obj["udp_over_tcp"].toBool();

        if (obj.contains("method")) method = obj["method"].toString();
        if (obj.contains("password")) password = obj["password"].toString();
        if (obj.contains("plugin")) plugin = obj["plugin"].toString();
        if (obj.contains("plugin_opts")) plugin_opts = obj["plugin_opts"].toString();
        if (obj.contains("uot"))
        {
            QJsonValue uot_obj = obj["uot"];
            if (uot_obj.isDouble()) uot = uot_obj.toInt();
            if (uot_obj.isBool()) uot = uot_obj.toBool();
            if (uot_obj.isObject()) uot = uot_obj.toObject()["enabled"].toBool();
        }

        mux_state = obj["multiplex"].isObject() ? (obj["multiplex"].toObject()["enabled"].toBool() ? 1 : 2) : 0;
        return true;
    }

    bool ShadowSocksBean::TryParseFromSIP008(const QJsonObject& object){
        if (object.isEmpty()) return false;
        TryParseJson(object);
        if (object.contains("remarks")) name = object["remarks"].toString();
        return !( this->serverAddress.isEmpty() || method.isEmpty() || password.isEmpty());
    }

    bool SocksHttpBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        socks_http_type = obj["type"] == "http" ? type_HTTP : type_Socks5;
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        username = obj["username"].toString();
        password = obj["password"].toString();
        parse_tls(stream, obj);
        return true;
    }

    bool SSHBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        user = obj["user"].toString();
        password = obj["password"].toString();
        privateKey = obj["private_key"].toString();
        privateKeyPath = obj["private_key_path"].toString();
        privateKeyPass = obj["private_key_passphrase"].toString();
        hostKey = QJsonArray2QListStr(obj["host_key"].toArray());
        hostKeyAlgs = QJsonArray2QListStr(obj["host_key_algorithms"].toArray());
        clientVersion = obj["client_version"].toString();

        return true;
    }

    bool TrojanVLESSBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        proxy_type = obj["type"].toString() == "trojan" ? proxy_Trojan : proxy_VLESS;
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        password = obj["password"].toString();
        if (proxy_type == proxy_VLESS) password = obj["uuid"].toString();
        flow = obj["flow"].toString();
        mux_state = obj["multiplex"].isObject() ? (obj["multiplex"].toObject()["enabled"].toBool() ? 1 : 2) : 0;

        stream->packet_encoding = obj["packet_encoding"].toString();

        parse_tls(stream, obj);
        parse_transport(stream, obj);
        return true;
    }

    bool VMessBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        uuid = obj["uuid"].toString();
        security = obj["security"].toString();
        aid = obj["alter_id"].toInt();
        mux_state = obj["multiplex"].isObject() ? (obj["multiplex"].toObject()["enabled"].toBool() ? 1 : 2) : 0;

        stream->packet_encoding = obj["packet_encoding"].toString();

        parse_tls(stream, obj);
        parse_transport(stream, obj);
        return true;
    }

    bool AnyTLSBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        password = obj["password"].toString();
        idle_session_check_interval = obj["idle_session_check_interval"].toString();
        idle_session_timeout = obj["idle_session_timeout"].toString();
        min_idle_session = obj["min_idle_session"].toInt();
        parse_tls(stream, obj);
        return true;
    }


    bool ShadowTLSBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        password = obj["password"].toString();
        shadowtls_version = obj["version"].toInt();
        parse_tls(stream, obj);
        return true;
    }

    bool WireguardBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        auto peers = obj["peers"].toArray();
        if (peers.empty()) return false;
        serverAddress = peers[0].toObject()["address"].toString();
        serverPort = peers[0].toObject()["port"].toInt();
        publicKey = peers[0].toObject()["public_key"].toString();
        reserved = QJsonArray2QListInt(peers[0].toObject()["reserved"].toArray());
        persistentKeepalive = peers[0].toObject()["persistent_keepalive_interval"].toInt();
        workerCount = obj["workers"].toInt();
        privateKey = obj["private_key"].toString();
        localAddress = QJsonArray2QListStr(obj["address"].toArray());
        MTU = obj["mtu"].toInt();
        useSystemInterface = obj["system"].toBool();

        junk_packet_count = obj["junk_packet_count"].toInt();
        junk_packet_min_size = obj["junk_packet_min_size"].toInt();
        junk_packet_max_size = obj["junk_packet_max_size"].toInt();
        init_packet_junk_size = obj["init_packet_junk_size"].toInt();
        response_packet_junk_size = obj["response_packet_junk_size"].toInt();
        init_packet_magic_header = obj["init_packet_magic_header"].toInt();
        response_packet_magic_header = obj["response_packet_magic_header"].toInt();
        underload_packet_magic_header = obj["underload_packet_magic_header"].toInt();
        transport_packet_magic_header = obj["transport_packet_magic_header"].toInt();
        if (junk_packet_count > 0 || junk_packet_min_size > 0 || junk_packet_max_size > 0)
        {
            enable_amnezia = true;
        }

        return true;
    }

    bool TailscaleBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        state_directory = obj["state_directory"].toString();
        auth_key = obj["auth_key"].toString();
        control_url = obj["control_url"].toString();
        ephemeral = obj["ephemeral"].toBool();
        hostname = obj["hostname"].toString();
        accept_routes = obj["accept_routes"].toBool();
        exit_node = obj["exit_node"].toString();
        exit_node_allow_lan_access = obj["exit_node_allow_lan_access"].toBool();
        advertise_routes = QJsonArray2QListStr(obj["advertise_routes"].toArray());
        advertise_exit_node = obj["advertise_exit_node"].toBool();

        return true;
    }

    bool ExtraCoreBean::TryParseJson(const QJsonObject& obj)
    {
        return false;
    }

    bool TorBean::TryParseJson(const QJsonObject &obj){
        name = obj["tag"].toString();
        executable_path = obj["executable_path"].toString();
        extra_args = QJsonArray2QListStr(obj["extra_args"].toArray());
        data_directory = obj["data_directory"].toString();
        torrc = obj["torrc"].toObject().toVariantMap();
        return true;
    };

    bool MieruBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        password = obj["password"].toString();
        username = obj["username"].toString();
        transport = obj["transport"].toString();
        multiplexing = obj["multiplexing"].toString();
        auto & ports = serverPorts;
        ports.clear();
        
        auto json_ports = obj["server_ports"];
        if (json_ports.isArray()){
            for (auto  val : obj["server_ports"].toArray()){
                ports.append(val.toString());
            };
        }

        return true;
    }
}
