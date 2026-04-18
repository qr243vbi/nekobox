#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class TrustTunnelBean : public AbstractBean {
    public:
        QString password = "";
        QString username = "";
        bool health_check = true;
        bool quic = false;
        std::shared_ptr<QUICEnum> quic_congestion_control;
        std::shared_ptr<V2rayStreamSettings> stream;

        TrustTunnelBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
            quic_congestion_control = std::make_shared<QUICEnum>("");
            stream = std::make_shared<V2rayStreamSettings>();
        }
        
        INIT_BEAN_MAP
            ADD_MAP("username", username, string);
            ADD_MAP("password", password, stringList);
            ADD_MAP("health_check", health_check, boolean);
            ADD_MAP("quic_enabled", quic, boolean);
            ADD_MAP("quic_congestion_control", quic_congestion_control, string);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP
/*/
        QString DisplayType() override { return "Tor"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        virtual QString type()const override {
            return "trusttunnel";
        };

    };
} // namespace Configs
