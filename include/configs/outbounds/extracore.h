#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class extracore : public outbound {
    public:
        QString socksAddress = "127.0.0.1";
        int socksPort;
        QString extraCorePath;
        QString extraCoreArgs;
        QString extraCoreConf;
        bool noLogs = false;

        extracore() : outbound() {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("socks_address", &socksAddress, itemType::string));
            _add(new configItem("socks_port", &socksPort, itemType::integer));
            _add(new configItem("extra_core_path", &extraCorePath, itemType::string));
            _add(new configItem("extra_core_args", &extraCoreArgs, itemType::string));
            _add(new configItem("extra_core_conf", &extraCoreConf, itemType::string));
            _add(new configItem("no_logs", &noLogs, itemType::boolean));
        };

        QString DisplayType() override { return "ExtraCore"; };

        BuildResult Build() override
        {
            QJsonObject socksOutbound;
            socksOutbound["type"] = "socks";
            socksOutbound["server"] = socksAddress;
            socksOutbound["server_port"] = socksPort;
            return {socksOutbound, ""};
        }
    };
}
