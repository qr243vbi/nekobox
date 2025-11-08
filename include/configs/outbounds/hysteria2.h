#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    class hysteria2 : public outbound
    {
        public:
        QStringList server_ports;
        QString hop_interval;
        int up_mbps = 0;
        int down_mbps = 0;
        QString obfsType = "salamander";
        QString obfsPassword;
        QString password;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

        hysteria2() : outbound()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("server_ports", &server_ports, stringList));
            _add(new configItem("hop_interval", &hop_interval, string));
            _add(new configItem("up_mbps", &up_mbps, integer));
            _add(new configItem("down_mbps", &down_mbps, integer));
            _add(new configItem("obfsType", &obfsType, string));
            _add(new configItem("obfsPassword", &obfsPassword, string));
            _add(new configItem("password", &password, string));
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
