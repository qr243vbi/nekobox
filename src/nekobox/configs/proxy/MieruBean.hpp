#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    INIT_ENUM(Multiplexing)
        ADD_ENUM_LIST(Preset::SingBox::MieruMultiplexing, 1);
    STOP_ENUM

    class MieruBean : public AbstractBean {
    public:
        QString password = "";
        QString username = "";
        std::shared_ptr<NetworkEnum> network ;
        std::shared_ptr<MultiplexingEnum> multiplexing ;
        QStringList serverPorts;
        QString traffic_pattern = "";

        MieruBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
            network = std::make_shared<NetworkEnum>("tcp");
            multiplexing = std::make_shared<MultiplexingEnum>("MULTIPLEXING_LOW");
        }
        
        INIT_BEAN_MAP
            ADD_MAP("password", password, string);
            ADD_MAP("username", username, string);
            ADD_MAP("multiplexing", multiplexing, string);
            ADD_MAP("transport", network, stringList);
            ADD_MAP("network", network, stringList);
            ADD_MAP("server_ports", serverPorts, stringList);
            ADD_MAP("traffic_pattern", traffic_pattern, string);
        STOP_MAP

 //       QString DisplayType() override { return "Mieru"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        virtual QString type()const override {
            return "mieru";
        };


        QString ToShareLink() const override;
    };
} // namespace Configs
