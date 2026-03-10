#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class MieruBean : public AbstractBean {
    public:
        QString password = "";
        QString username = "";
        QString transport = "TCP";
        QString multiplexing = "MULTIPLEXING_LOW";
        QStringList serverPorts;
        QString traffic_pattern = "";

        MieruBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
        }
        
        INIT_MAP
            ADD_MAP("password", password, string);
            ADD_MAP("username", username, string);
            ADD_MAP("multiplexing", multiplexing, string);
            ADD_MAP("transport", transport, stringList);
            ADD_MAP("server_ports", serverPorts, stringList);
            ADD_MAP("traffic_pattern", traffic_pattern, string);
        STOP_MAP

 //       QString DisplayType() override { return "Mieru"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);
        #ifdef DEBUG_MODE
        virtual QString type() override {
            return "mieru";
        };
        #endif

        QString ToShareLink() const override;
    };
} // namespace Configs
