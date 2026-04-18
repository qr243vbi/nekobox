#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class JuicityBean : public AbstractBean {
    public:
        QString password = "";
        QString username = "";
        std::shared_ptr<V2rayStreamSettings> stream;

        JuicityBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
            stream = std::make_shared<V2rayStreamSettings>();
        }
        
        INIT_BEAN_MAP
            ADD_MAP("username", username, string);
            ADD_MAP("password", password, string);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP

        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        virtual QString type()const override {
            return "juicity";
        };
    };
} // namespace Configs
