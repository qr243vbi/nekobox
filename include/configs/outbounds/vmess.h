#pragma once
#include "include/configs/baseConfig.h"
#include "include/configs/common/multiplex.h"
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"
#include "include/configs/common/transport.h"

namespace Configs
{

    inline QStringList vmessSecurity = {"auto", "none", "zero", "aes-128-gcm", "chacha20-poly1305"};
    inline QStringList vPacketEncoding = {"", "packetaddr", "xudp"};

    class vmess : public baseConfig
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();
        QString uuid;
        QString security = "auto";
        int alter_id = 0;
        bool global_padding = false;
        bool authenticated_length = false;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();
        QString packet_encoding;
        std::shared_ptr<Transport> transport = std::make_shared<Transport>();
        std::shared_ptr<Multiplex> mux = std::make_shared<Multiplex>();

        vmess()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("uuid", &uuid, string));
            _add(new configItem("security", &security, string));
            _add(new configItem("alter-id", &alter_id, integer));
            _add(new configItem("global-padding", &global_padding, boolean));
            _add(new configItem("authenticated_length", &authenticated_length, boolean));
            _add(new configItem("tls", dynamic_cast<JsonStore *>(tls.get()), jsonStore));
            _add(new configItem("packet_encoding", &packet_encoding, string));
            _add(new configItem("transport", dynamic_cast<JsonStore *>(transport.get()), jsonStore));
            _add(new configItem("mux", dynamic_cast<JsonStore *>(mux.get()), jsonStore));
        }
    };
}
