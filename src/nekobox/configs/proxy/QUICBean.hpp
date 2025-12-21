#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class QUICBean : public AbstractBean {
    public:
        static constexpr int proxy_Hysteria = 0;
        static constexpr int proxy_TUIC = 1;
        static constexpr int proxy_Hysteria2 = 3;
        int proxy_type = proxy_Hysteria;

        bool forceExternal = false;

        // Hysteria 1

        static constexpr int hysteria_protocol_udp = 0;
        static constexpr int hysteria_protocol_facktcp = 1;
        static constexpr int hysteria_protocol_wechat_video = 2;
        int hyProtocol = 0;

        static constexpr int hysteria_auth_none = 0;
        static constexpr int hysteria_auth_string = 1;
        static constexpr int hysteria_auth_base64 = 2;
        int authPayloadType = 0;
        QString authPayload = "";

        // Hysteria 1&2

        QString obfsPassword = "";

        int uploadMbps = 100;
        int downloadMbps = 100;

        qint64 streamReceiveWindow = 0;
        qint64 connectionReceiveWindow = 0;
        bool disableMtuDiscovery = false;

        QStringList serverPorts;
        QString hop_interval;

        // TUIC

        QString uuid = "";
        QString congestionControl = "bbr";
        QString udpRelayMode = "native";
        bool zeroRttHandshake = false;
        QString heartbeat = "10s";
        bool uos = false;

        // HY2&TUIC

        QString password = "";

        // TLS

        bool allowInsecure = false;
        QString sni = "";
        QString alpn = "";
        QString caText = "";
        bool disableSni = false;

        #undef _add
        #define _add(X, Y, B, T) _put(X, Y, &this->B, ITEM_TYPE(T));

        virtual ConfJsMap  _map() override{
            static ConfJsMapStat 
                hys1 , 
                hys2 , 
                tuic ;
            static bool init = false;
            if (!init){
                init = true;
                // Add base AbstractBean fields
                _add(tuic, "name", name, string);
                _add(tuic, "addr", serverAddress, string);
                _add(tuic, "port", serverPort, integer);
                
                _add(tuic, "forceExternal", forceExternal, boolean);
            // TLS
                _add(tuic, "allowInsecure",  allowInsecure, boolean);
                _add(tuic, "sni", sni, string);
                _add(tuic, "alpn", alpn, string);
                _add(tuic, "caText", caText, string);
                _add(tuic, "disableSni", disableSni, boolean);

                hys1.insert(tuic);
                _add(hys1, "authPayload", authPayload, string);
                _add(hys1, "obfsPassword", obfsPassword, string);
                _add(hys1, "uploadMbps", uploadMbps, integer);
                _add(hys1, "downloadMbps", downloadMbps, integer);
                _add(hys1, "streamReceiveWindow", streamReceiveWindow, integer64);
                _add(hys1, "connectionReceiveWindow", connectionReceiveWindow, integer64);
                _add(hys1, "disableMtuDiscovery", disableMtuDiscovery, boolean);
                _add(hys1, "server_ports", serverPorts, stringList);
                _add(hys1, "hop_interval", hop_interval, string);                

                hys2.insert(hys1);

                _add(hys1, "authPayloadType", authPayloadType, integer);
                _add(hys1, "protocol", hyProtocol, integer);

                _add(hys2, "password", password, string);

                _add(tuic, "uuid", uuid, string);
                _add(tuic, "password", password, string);
                _add(tuic, "congestionControl", congestionControl, string);
                _add(tuic, "udpRelayMode", udpRelayMode, string);
                _add(tuic, "zeroRttHandshake", zeroRttHandshake, boolean);
                _add(tuic, "heartbeat", heartbeat, string);
                _add(tuic, "uos", uos, boolean);

            }
            if (proxy_type == proxy_TUIC) {
                return tuic;
            } else if (proxy_type == proxy_Hysteria) {
                return hys1;
            } else {
                return hys2;
            }
        }

        #undef _add

        explicit QUICBean(int _proxy_type) : AbstractBean(0) {
            proxy_type = _proxy_type;
            if (proxy_type == proxy_Hysteria || proxy_type == proxy_Hysteria2) {
                if (proxy_type == proxy_Hysteria) { // hy1
                } else { // hy2
                    uploadMbps = 0;
                    downloadMbps = 0;
                }
            } else if (proxy_type == proxy_TUIC) {
            }
        };

        QString DisplayAddress() override {
            return ::DisplayAddress(serverAddress, serverPort);
        }

        QString DisplayType() override {
            if (proxy_type == proxy_TUIC) {
                return "TUIC";
            } else if (proxy_type == proxy_Hysteria) {
                return "Hysteria1";
            } else {
                return "Hysteria2";
            }
        };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs