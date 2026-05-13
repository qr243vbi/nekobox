

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


    bool QUICBean::TryParseJson(const QJsonObject& obj)
    {
            using namespace Configs::From_Json;
        auto type = obj["type"].toString();
        add_network(this, obj);
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
            add_default_fields(this->entity, obj);
            alpn = obj["tls"].toObject()["alpn"].isArray() ? QJsonArray2QListStr(obj["tls"].toObject()["alpn"].toArray()).join(",") : obj["tls"].toObject()["alpn"].toString();
            sni = obj["tls"].toObject()["server_name"].toString();
            disableSni = obj["tls"].toObject()["disable_sni"].toBool();
            allowInsecure = obj["tls"].toObject()["insecure"].toBool();
            return true;
        }
        return false;
    }

    bool QUICBean::TryParseLink(const QString &link) {
        using namespace From_Link;
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
}