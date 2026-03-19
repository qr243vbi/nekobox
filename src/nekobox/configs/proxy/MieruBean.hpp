#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
<<<<<<< HEAD
=======
    INIT_ENUM(Multiplexing)
        ADD_ENUM_LIST(Preset::SingBox::MieruMultiplexing, 1);
    STOP_ENUM

    INIT_ENUM(Transport)
        ADD_ENUM_LIST(Preset::SingBox::MieruTransport, 1);
    STOP_ENUM

>>>>>>> other-repo/main
    class MieruBean : public AbstractBean {
    public:
        QString password = "";
        QString username = "";
<<<<<<< HEAD
        QString transport = "TCP";
        QString multiplexing = "MULTIPLEXING_LOW";
        QStringList serverPorts;

        MieruBean() : AbstractBean(0) {
=======
        std::shared_ptr<TransportEnum> transport ;
        std::shared_ptr<MultiplexingEnum> multiplexing ;
        QStringList serverPorts;
        QString traffic_pattern = "";

        MieruBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
            transport = std::make_shared<TransportEnum>("TCP");
            multiplexing = std::make_shared<MultiplexingEnum>("MULTIPLEXING_LOW");
>>>>>>> other-repo/main
        }
        
        INIT_MAP
            ADD_MAP("password", password, string);
            ADD_MAP("username", username, string);
            ADD_MAP("multiplexing", multiplexing, string);
            ADD_MAP("transport", transport, stringList);
            ADD_MAP("server_ports", serverPorts, stringList);
<<<<<<< HEAD
        STOP_MAP

        QString DisplayType() override { return "Mieru"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
=======
            ADD_MAP("traffic_pattern", traffic_pattern, string);
        STOP_MAP

 //       QString DisplayType() override { return "Mieru"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;
        #ifdef DEBUG_MODE
        virtual QString type()const override {
            return "mieru";
        };
        #endif

        QString ToShareLink() const override;
>>>>>>> other-repo/main
    };
} // namespace Configs
