#pragma once
#include "DialFields.h"
#include "include/configs/baseConfig.h"

namespace Configs
{
    class OutboundCommons : public baseConfig
    {
        public:
        QString type;
        QString name;
        int id = -1;
        QString server;
        int server_port = 0;
        std::shared_ptr<DialFields> dialFields = std::make_shared<DialFields>();

        OutboundCommons()
        {
            _add(new configItem("type", &type, string));
            _add(new configItem("name", &name, string));
            _add(new configItem("id", &id, integer));
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
}
