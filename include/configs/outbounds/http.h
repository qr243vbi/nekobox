#pragma once
#include "include/configs/baseConfig.h"
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    class http : public baseConfig
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();
        QString username;
        QString password;
        QString path;
        QStringList headers;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

        http()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("username", &username, string));
            _add(new configItem("password", &password, string));
            _add(new configItem("path", &path, string));
            _add(new configItem("headers", &headers, stringList));
            _add(new configItem("tls", dynamic_cast<JsonStore *>(tls.get()), jsonStore));
        }
    };
}
