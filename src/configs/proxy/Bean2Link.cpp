#include "nekobox/configs/proxy/V2RayStreamSettings.hpp"
#include "nekobox/dataStore/ProxyEntity.hpp"
#include "nekobox/configs/proxy/includes.h"

#include <QUrlQuery>
#include <QProcess>
#include <qurlquery.h>

namespace Configs {
    QString SocksHttpBean::ToShareLink() {
        QUrl url;
        if (socks_http_type == type_HTTP) { // http
            if (stream->security == "tls") {
                url.setScheme("https");
            } else {
                url.setScheme("http");
            }
        } else {
            url.setScheme(QString("socks%1").arg(socks_http_type));
        }
        if (!name.isEmpty()) url.setFragment(name);
        if (!username.isEmpty()) url.setUserName(username);
        if (!password.isEmpty()) url.setPassword(password);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        return url.toString(QUrl::FullyEncoded);
    }

    static void add_security(std::shared_ptr<V2rayStreamSettings> stream, QUrlQuery & query){
        auto security = stream->security;
        if (security == "tls" && !stream->reality_pbk.trimmed().isEmpty()) security = "reality";
        query.addQueryItem("security", security == "" ? "none" : security);

        add_query_nonempty("sni", query, stream->sni);
        add_query_nonempty("alpn", query, stream->alpn);
        if (stream->allow_insecure) query.addQueryItem("insecure", "1");
        add_query_nonempty("fp", query, stream->utlsFingerprint);
        if (stream->enable_tls_fragment) query.addQueryItem("fragment", "1");
        add_query_nonempty("fragment_fallback_delay", query, stream->tls_fragment_fallback_delay);
        if (stream->enable_tls_record_fragment) query.addQueryItem("record_fragment", "1");

        if (security == "reality") {
            query.addQueryItem("pbk", stream->reality_pbk);
            add_query_nonempty("sid", query, stream->reality_sid);
        }        
    }

    QString ShadowTLSBean::ToShareLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("shadowtls");
        url.setUserName(password);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);

        add_query_int("version", query, shadowtls_version);

        //  security
        add_security(stream, query);

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QString AnyTLSBean::ToShareLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("anytls");
        url.setUserName(password);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);

        add_query_nonempty("idle_session_check_interval", query, idle_session_check_interval);
        add_query_nonempty("idle_session_timeout", query, idle_session_timeout);
        add_query_int_natural("min_idle_session", query, min_idle_session);

        //  security
        add_security(stream, query);

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QString TrojanVLESSBean::ToShareLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme(proxy_type == proxy_VLESS ? "vless" : "trojan");
        url.setUserName(password);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);

        //  security
        add_security(stream, query);

        // type
        query.addQueryItem("type", stream->network);

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
                query.addQueryItem("headerType", "http");
                query.addQueryItem("host", stream->host);
            }
        } else if (stream->network == "xhttp") {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
            add_query_nonempty("mode", query, stream->xhttp_mode);
            add_query_nonempty("extra", query, stream->xhttp_extra);
        }

        // mux
        if (mux_state == 1) {
            query.addQueryItem("mux", "true");
        } else if (mux_state == 2) {
            query.addQueryItem("mux", "false");
        }

        // protocol
        if (proxy_type == proxy_VLESS) {
            add_query_nonempty("flow", query, flow);
            add_query_nonempty("packetEncoding", query, stream->packet_encoding);
            query.addQueryItem("encryption", "none");
        }

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    const char* fixShadowsocksUserNameEncodeMagic = "fixShadowsocksUserNameEncodeMagic-holder-for-QUrl";

    QString ShadowSocksBean::ToShareLink() {
        QUrl url;
        url.setScheme("ss");
        if (method.startsWith("2022-")) {
            url.setUserName(fixShadowsocksUserNameEncodeMagic);
        } else {
            auto method_password = method + ":" + password;
            url.setUserName(method_password.toUtf8().toBase64(QByteArray::Base64Option::Base64UrlEncoding));
        }
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);
        QUrlQuery query;
        add_query_nonempty("plugin", query, plugin);

        // mux
        if (mux_state == 1) {
            query.addQueryItem("mux", "true");
        } else if (mux_state == 2) {
            query.addQueryItem("mux", "false");
        }
        // uot
        if (uot == 1) {
            query.addQueryItem("uot", "1");
        } else if (uot == 2) {
            query.addQueryItem("uot", "2");
        }

        if (!query.isEmpty()) url.setQuery(query);
        //
        auto link = url.toString(QUrl::FullyEncoded);
        link = link.replace(fixShadowsocksUserNameEncodeMagic, method + ":" + QUrl::toPercentEncoding(password));
        return link;
    }

    QString VMessBean::ToShareLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("vmess");
        url.setUserName(uuid);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);

        query.addQueryItem("encryption", security);

        //  security
        auto security = stream->security;
        if (security == "tls" && !stream->reality_pbk.trimmed().isEmpty()) security = "reality";
        query.addQueryItem("security", security == "" ? "none" : security);

        if (!stream->sni.isEmpty()) query.addQueryItem("sni", stream->sni);
        if (stream->allow_insecure) query.addQueryItem("allowInsecure", "1");
        if (stream->utlsFingerprint.isEmpty()) {
            query.addQueryItem("fp", Configs::dataStore->utlsFingerprint);
        } else {
            query.addQueryItem("fp", stream->utlsFingerprint);
        }
        if (stream->enable_tls_fragment) query.addQueryItem("fragment", "1");
        if (!stream->tls_fragment_fallback_delay.isEmpty()) query.addQueryItem("fragment_fallback_delay", stream->tls_fragment_fallback_delay);
        if (stream->enable_tls_record_fragment) query.addQueryItem("record_fragment", "1");

        if (security == "reality") {
            query.addQueryItem("pbk", stream->reality_pbk);
            if (!stream->reality_sid.isEmpty()) query.addQueryItem("sid", stream->reality_sid);
        }

        // type
        query.addQueryItem("type", stream->network);

        if (stream->network == "ws" || stream->network == "http" || stream->network == "httpupgrade") {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
        } else if (stream->network == "grpc") {
            add_query_nonempty("serviceName", query, stream->path);
        } else if (stream->network == "tcp") {
            if (stream->header_type == "http") {
                query.addQueryItem("headerType", "http");
                query.addQueryItem("host", stream->host);
            }
        } else if (stream->network == "xhttp") {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
            add_query_nonempty("mode", query, stream->xhttp_mode);
            add_query_nonempty("extra", query, stream->xhttp_extra);
        }

        // mux
        if (mux_state == 1) {
            query.addQueryItem("mux", "true");
        } else if (mux_state == 2) {
            query.addQueryItem("mux", "false");
        }

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QString QUICBean::ToShareLink() {
        QUrl url;
        QString portRange;
        if (proxy_type == proxy_Hysteria) {
            url.setScheme("hysteria");
            url.setHost(serverAddress);
            url.setPort(0);
            QUrlQuery query;
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
                url.setPort(serverPort);
            if (!hop_interval.isEmpty()) query.addQueryItem("hop_interval", hop_interval);
            if (!query.isEmpty()) url.setQuery(query);
            if (!name.isEmpty()) url.setFragment(name);
        } else if (proxy_type == proxy_TUIC) {
            url.setScheme("tuic");
            url.setUserName(uuid);
            url.setPassword(password);
            url.setHost(serverAddress);
            url.setPort(serverPort);

            QUrlQuery q;
            if (!congestionControl.isEmpty()) q.addQueryItem("congestion_control", congestionControl);
            if (!alpn.isEmpty()) q.addQueryItem("alpn", alpn);
            if (!sni.isEmpty()) q.addQueryItem("sni", sni);
            if (!udpRelayMode.isEmpty()) q.addQueryItem("udp_relay_mode", udpRelayMode);
            if (allowInsecure) q.addQueryItem("allow_insecure", "1");
            if (disableSni) q.addQueryItem("disable_sni", "1");
            if (!q.isEmpty()) url.setQuery(q);
            if (!name.isEmpty()) url.setFragment(name);
        } else if (proxy_type == proxy_Hysteria2) {
            url.setScheme("hy2");
            url.setHost(serverAddress);
            url.setPort(0);
            if (password.contains(":")) {
                url.setUserName(SubStrBefore(password, ":"));
                url.setPassword(SubStrAfter(password, ":"));
            } else {
                url.setUserName(password);
            }
            QUrlQuery q;
            if (!obfsPassword.isEmpty()) {
                q.addQueryItem("obfs", "salamander");
                q.addQueryItem("obfs-password", QUrl::toPercentEncoding(obfsPassword));
            }
            if (allowInsecure) q.addQueryItem("insecure", "1");
            if (!sni.isEmpty()) q.addQueryItem("sni", sni);
            if (!serverPorts.empty()) {
                QStringList portList;
                for (const auto& range : serverPorts) {
                    QString modifiedRange = range;
                    modifiedRange.replace(":", "-");
                    portList.append(modifiedRange);
                }
                portRange = portList.join(",");
            } else
                url.setPort(serverPort);
            if (!hop_interval.isEmpty()) q.addQueryItem("hop_interval", hop_interval);
            if (!q.isEmpty()) url.setQuery(q);
            if (!name.isEmpty()) url.setFragment(name);
        }
        if (portRange.isEmpty())
            return url.toString(QUrl::FullyEncoded);
        else
            return url.toString(QUrl::FullyEncoded).replace(":0?", ":" + portRange + "?");
    }

    QString WireguardBean::ToShareLink() {
        QUrl url;
        url.setScheme("wg");
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);
        QUrlQuery query;
        query.addQueryItem("private_key", privateKey);
        query.addQueryItem("peer_public_key", publicKey);
        query.addQueryItem("pre_shared_key", preSharedKey);
        query.addQueryItem("reserved", FormatReserved());
        add_query_int("persistent_keepalive", query, (persistentKeepalive));
        add_query_int("mtu", query, (MTU));
        query.addQueryItem("use_system_interface", useSystemInterface ? "true":"false");
        query.addQueryItem("local_address", localAddress.join("-"));
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

    QString TailscaleBean::ToShareLink()
    {
        QUrl url;
        url.setScheme("ts");
        url.setHost("tailscale");
        if (!name.isEmpty()) url.setFragment(name);
        QUrlQuery q;
        add_query_nonempty("state_directory", q, QUrl::toPercentEncoding(state_directory));
        add_query_nonempty("auth_key", q, QUrl::toPercentEncoding(auth_key));
        add_query_nonempty("control_url", q, QUrl::toPercentEncoding(control_url));
        add_query_nonempty("ephemeral", q, ephemeral ? "true" : "false");
        add_query_nonempty("hostname", q, QUrl::toPercentEncoding(hostname));
        add_query_nonempty("accept_routes", q, accept_routes ? "true" : "false");
        add_query_nonempty("exit_node", q, exit_node);
        add_query_nonempty("exit_node_allow_lan_access", q, exit_node_allow_lan_access ? "true" : "false");
        add_query_nonempty("advertise_routes", q, QUrl::toPercentEncoding(advertise_routes.join(",")));
        add_query_nonempty("advertise_exit_node", q, advertise_exit_node ? "true" : "false");
        add_query_nonempty("global_dns", q, globalDNS ? "true" : "false");
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString SSHBean::ToShareLink() {
        QUrl url;
        url.setScheme("ssh");
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);
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

    QString ExtraCoreBean::ToShareLink()
    {
        return "Unsupported for now";
    }

    QString TorBean::ToShareLink(){
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

    QString MieruBean::ToShareLink(){
        QUrl url;
        url.setScheme("mieru");
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);
        QUrlQuery q;
        add_query_nonempty( "username", q, username);
        add_query_nonempty( "password", q, password);
        add_query_nonempty( "transport", q, "TCP");
        add_query_nonempty( "multiplexing", q, multiplexing);
        add_query_nonempty( "server_ports", q, serverPorts.join(","));
        url.setQuery(q);

        return url.toString(QUrl::FullyEncoded);
    }

} // namespace Configs
