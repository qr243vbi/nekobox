#pragma once
#include "include/configs/baseConfig.h"

namespace Configs
{
    class DialFields : public baseConfig
    {
        public:
        bool reuse_addr = false;
        QString connect_timeout;
        bool tcp_fast_open = false;
        bool tcp_multi_path = false;
        bool udp_fragment = false;

        DialFields()
        {
            _add(new configItem("reuse_addr", &reuse_addr, itemType::boolean));
            _add(new configItem("connect_timeout", &connect_timeout, string));
            _add(new configItem("tcp_fast_open", &tcp_fast_open, itemType::boolean));
            _add(new configItem("tcp_multi_path", &tcp_multi_path, itemType::boolean));
            _add(new configItem("udp_fragment", &udp_fragment, itemType::boolean));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
