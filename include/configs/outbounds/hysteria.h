#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    class hysteria : public outbound
    {
        public:
        QStringList server_ports;
        QString hop_interval;
        int up_mbps = 0;
        int down_mbps = 0;
        QString obfs;
        QString auth;
        QString auth_str;
        int recv_window_conn = 0;
        int recv_window = 0;
        bool disable_mtu_discovery = false;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

        hysteria() : outbound()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("server_ports", &server_ports, stringList));
            _add(new configItem("hop_interval", &hop_interval, string));
            _add(new configItem("up_mbps", &up_mbps, integer));
            _add(new configItem("down_mbps", &down_mbps, integer));
            _add(new configItem("obfs", &obfs, string));
            _add(new configItem("auth", &auth, string));
            _add(new configItem("auth_str", &auth_str, string));
            _add(new configItem("recv_window_conn", &recv_window_conn, integer));
            _add(new configItem("recv_window", &recv_window, integer));
            _add(new configItem("disable_mtu_discovery", &disable_mtu_discovery, boolean));
            _add(new configItem("tls", dynamic_cast<JsonStore *>(tls.get()), jsonStore));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
    };
}
