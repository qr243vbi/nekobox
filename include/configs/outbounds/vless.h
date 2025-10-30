#pragma once
#include "include/configs/baseConfig.h"
#include "include/configs/common/multiplex.h"
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"
#include "include/configs/common/transport.h"

namespace Configs
{
    inline QStringList vlessFlows = {"xtls-rprx-vision"};

    class vless : public baseConfig, public outboundMeta
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();
        QString uuid;
        QString flow;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();
        QString packet_encoding;
        std::shared_ptr<Multiplex> mux = std::make_shared<Multiplex>();
        std::shared_ptr<Transport> transport = std::make_shared<Transport>();

        vless()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("uuid", &uuid, string));
            _add(new configItem("flow", &flow, string));
            _add(new configItem("tls", dynamic_cast<JsonStore *>(tls.get()), jsonStore));
            _add(new configItem("packet_encoding", &packet_encoding, string));
            _add(new configItem("mux", dynamic_cast<JsonStore *>(mux.get()), jsonStore));
            _add(new configItem("transport", dynamic_cast<JsonStore *>(transport.get()), jsonStore));
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
