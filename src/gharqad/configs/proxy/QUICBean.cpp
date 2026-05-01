#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {




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