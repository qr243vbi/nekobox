#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class CustomBean : public AbstractBean {
    public:
        QString core;
        QList<QString> command;
        QString config_suffix;
        QString config_simple;
        int mapping_port = 0;
        int socks_port = 0;

        CustomBean() : AbstractBean(0) {
        }
        INIT_MAP
            ADD_MAP("core", core, string);
            ADD_MAP("cmd", command, stringList);
            ADD_MAP("cs", config_simple, string);
            ADD_MAP("cs_suffix", config_suffix, string);
            ADD_MAP("mapping_port", mapping_port, integer);
            ADD_MAP("socks_port", socks_port, integer);
        STOP_MAP

        QString DisplayType() override {
            if (core == "internal") {
                auto type = QString2QJsonObject(config_simple)["type"].toString();
                if (!type.isEmpty()) type[0] = type[0].toUpper();
                return type.isEmpty() ? "Custom Outbound" : "Custom " + type + " Outbound";
            } else if (core == "internal-full") {
                return "Custom Config";
            }
            return core;
        };

        QString DisplayCoreType() override { return software_core_name; };

        QString DisplayAddress() override {
            if (core == "internal") {
                auto obj = QString2QJsonObject(config_simple);
                return ::DisplayAddress(obj["server"].toString(), obj["server_port"].toInt());
            } else if (core == "internal-full") {
                return {};
            }
            return AbstractBean::DisplayAddress();
        };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;
    };
} // namespace Configs