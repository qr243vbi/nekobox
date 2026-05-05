#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {




    QString QUICBean::ToShareLink() const {
            using namespace Configs::To_Link;

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

    CoreObjOutboundBuildResult QUICBean::BuildCoreObjSingBox() const {
        using namespace To_CoreObj_box;
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
}