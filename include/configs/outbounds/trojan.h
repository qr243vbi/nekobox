#pragma once
#include "include/configs/baseConfig.h"
#include "include/configs/common/DialFields.h"
#include "include/configs/common/multiplex.h"
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"
#include "include/configs/common/transport.h"

namespace Configs
{
    class Trojan : public baseConfig, public outboundMeta
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();
        QString password;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();
        std::shared_ptr<Multiplex> mux = std::make_shared<Multiplex>();
        std::shared_ptr<Transport> transport = std::make_shared<Transport>();

        Trojan()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("password", &password, string));
            _add(new configItem("tls", dynamic_cast<JsonStore *>(tls.get()), jsonStore));
            _add(new configItem("mux", dynamic_cast<JsonStore *>(mux.get()), jsonStore));
            _add(new configItem("transport", dynamic_cast<JsonStore *>(transport.get()), jsonStore));
        }
    };
}
