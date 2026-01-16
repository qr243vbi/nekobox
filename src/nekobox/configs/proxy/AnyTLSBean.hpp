#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class AnyTLSBean : public AbstractBean {
    public:
        QString password = "";
        QString idle_session_check_interval = "30s";
        QString idle_session_timeout = "30s";
        int min_idle_session = 0;

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        AnyTLSBean() : AbstractBean(0) {
        }
        
        INIT_MAP
            ADD_MAP("password", password, string);
            ADD_MAP("session_idle_check_interval", idle_session_check_interval, string);
            ADD_MAP("session_idle_timeout", idle_session_timeout, string);
            ADD_MAP("min_idle_session", min_idle_session, integer);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP

        QString DisplayType() override { return "AnyTLS"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
