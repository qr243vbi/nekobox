#pragma once
<<<<<<< HEAD

=======
#include "include/configs/baseConfig.h"
>>>>>>> main
#include "include/configs/common/Outbound.h"

namespace Configs
{
<<<<<<< HEAD
    class socks : public outbound
    {
        public:
        QString username;
        QString password;
        int version = 5;
        bool uot = false;

        socks() : outbound()
=======
    class socks : public baseConfig
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();
        QString username;
        QString password;
        bool uot = false;

        socks()
>>>>>>> main
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("username", &username, string));
            _add(new configItem("password", &password, string));
<<<<<<< HEAD
            _add(new configItem("version", &version, integer));
            _add(new configItem("uot", &uot, boolean));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
=======
            _add(new configItem("uot", &uot, boolean));
        }
>>>>>>> main
    };
}
