#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class VMessBean : public AbstractBean {
    private:
        V2rayStreamSettings * streamPtr;
    public:
        QString uuid = "";
        int aid = 0;
        QString security = "auto";

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        VMessBean() : AbstractBean(0) {
            streamPtr = stream.get();
        }
        #define _add(X, Y, B) ADD_MAP(X, Y, B)

        INIT_MAP
            ADD_MAP("id", uuid, string);
            ADD_MAP("aid", aid, integer);
            ADD_MAP("sec", security, string);
            ADD_MAP("stream", streamPtr, jsonStore);
        STOP_MAP

        QString DisplayType() override { return "VMess"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
