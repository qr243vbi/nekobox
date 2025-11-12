#pragma once
#include "DialFields.h"
#include "include/configs/baseConfig.h"

namespace Configs
{
    class OutboundCommons : public baseConfig
    {
        public:
        QString name;
        QString server;
        int server_port = 0;
        std::shared_ptr<DialFields> dialFields = std::make_shared<DialFields>();

        OutboundCommons()
        {
            _add(new configItem("name", &name, string));
            _add(new configItem("server", &server, string));
            _add(new configItem("server_port", &server_port, integer));
            _add(new configItem("dial_fields", dynamic_cast<JsonStore *>(dialFields.get()), jsonStore));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class outbound : public baseConfig
    {
    public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();

        void ResolveDomainToIP(const std::function<void()> &onFinished);

        virtual QString GetAddress()
        {
            return commons->server;
        }

        virtual QString DisplayAddress()
        {
            return ::DisplayAddress(commons->server, commons->server_port);
        }

        virtual QString DisplayName()
        {
            if (commons->name.isEmpty()) {
                return DisplayAddress();
            }
            return commons->name;
        }

        virtual QString DisplayType() { return {}; };

        QString DisplayTypeAndName()
        {
            return QString("[%1] %2").arg(DisplayType(), DisplayName());
        }

        virtual bool IsEndpoint() { return false; };
    };
}
