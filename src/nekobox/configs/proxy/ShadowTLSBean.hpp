#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class ShadowTLSBean : public AbstractBean {
    public:
        QString password = "";
        int shadowtls_version = 1;

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        ShadowTLSBean() : AbstractBean(0) {
        }
        
        INIT_MAP
            ADD_MAP("password", password, string);
            ADD_MAP("shadowtls_version", shadowtls_version, integer);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP

        QString DisplayType() override { return "ShadowTLS"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
