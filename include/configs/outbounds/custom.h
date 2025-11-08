#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class Custom : public outbound
    {
    public:
        QString config;
        QString type; // one of outbound or fullconfig TODO use consts

        Custom() : outbound()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("config", &config, string));
            _add(new configItem("type", &type, string));
        }

        QString GetAddress() override
        {
            if (type == "outbound") {
                auto obj = QString2QJsonObject(config);
                return obj["server"].toString();
            }
            return {};
        }

        QString DisplayAddress() override
        {
            if (type == "outbound") {
                auto obj = QString2QJsonObject(config);
                return ::DisplayAddress(obj["server"].toString(), obj["server_port"].toInt());
            }
            return {};
        }

        QString DisplayType() override
        {
            if (type == "outbound") {
                auto outboundType = QString2QJsonObject(config)["type"].toString();
                if (!outboundType.isEmpty()) outboundType[0] = outboundType[0].toUpper();
                return outboundType.isEmpty() ? "Custom Outbound" : "Custom " + outboundType + " Outbound";
            } else if (type == "fullconfig") {
                return "Custom Config";
            }
            return type;
        };

        BuildResult Build() override
        {
            return {QString2QJsonObject(config), ""};
        }
    };
}
