#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class MieruBean : public AbstractBean {
    public:
        QString password = "";
        QString username = "";
        QString multiplexing = "MULTIPLEXING_LOW";
        QStringList serverPorts;

        MieruBean() : AbstractBean(0) {
        }
        
        INIT_MAP
            ADD_MAP("password", password, string);
            ADD_MAP("username", username, string);
            ADD_MAP("multiplexing", multiplexing, string);
            ADD_MAP("server_ports", serverPorts, stringList);
        STOP_MAP

        QString DisplayType() override { return "Mieru"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
