#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class AnyTLSBean : public AbstractBean {
    private:
        V2rayStreamSettings * streamPtr;
    public:
        QString password = "";
        int idle_session_check_interval = 30;
        int idle_session_timeout = 30;
        int min_idle_session = 0;

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        AnyTLSBean() : AbstractBean(0) {
            streamPtr = stream.get();
        }
        
        INIT_MAP
            ADD_MAP("password", password, string);
            ADD_MAP("idle_session_check_interval", idle_session_check_interval, integer);
            ADD_MAP("idle_session_timeout", idle_session_timeout, integer);
            ADD_MAP("min_idle_session", min_idle_session, integer);
            ADD_MAP("stream", streamPtr, jsonStore);
        STOP_MAP

        QString DisplayType() override { return "AnyTLS"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
