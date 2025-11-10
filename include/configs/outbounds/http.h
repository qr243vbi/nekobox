#pragma once
<<<<<<< HEAD

=======
#include "include/configs/baseConfig.h"
>>>>>>> main
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
<<<<<<< HEAD
    class http : public outbound
    {
        public:
=======
    class http : public baseConfig
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();
>>>>>>> main
        QString username;
        QString password;
        QString path;
        QStringList headers;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

<<<<<<< HEAD
        http() : outbound()
=======
        http()
>>>>>>> main
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("username", &username, string));
            _add(new configItem("password", &password, string));
            _add(new configItem("path", &path, string));
            _add(new configItem("headers", &headers, stringList));
            _add(new configItem("tls", dynamic_cast<JsonStore *>(tls.get()), jsonStore));
        }
<<<<<<< HEAD

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
=======
>>>>>>> main
    };
}
