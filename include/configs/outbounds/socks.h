#pragma once
#include "include/configs/baseConfig.h"
#include "include/configs/common/DialFields.h"
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class socks : public baseConfig, public outboundMeta
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();
        QString username;
        QString password;
        bool uot = false;

        socks()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("username", &username, string));
            _add(new configItem("password", &password, string));
            _add(new configItem("uot", &uot, boolean));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        // outboundMeta overrides
        QString DisplayAddress() override;
        QString DisplayName() override;
        QString DisplayType() override;
        QString DisplayTypeAndName() override;
        bool IsEndpoint() override;
    };
}
