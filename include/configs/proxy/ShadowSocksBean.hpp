#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class ShadowSocksBean : public AbstractBean {
    private:
        V2rayStreamSettings * streamPtr;
    public:
        QString method = "aes-128-gcm";
        QString password = "";
        QString plugin = "";
        int uot = 0;

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        ShadowSocksBean() : AbstractBean(0) {
            streamPtr = stream.get();
        }

        INIT_MAP
            ADD_MAP("method", method, string);
            ADD_MAP("pass", password, string);
            ADD_MAP("plugin", plugin, string);
            ADD_MAP("uot", uot, integer);
            ADD_MAP("stream", streamPtr, jsonStore);
        STOP_MAP

        bool IsValid() {
            return Preset::SingBox::ShadowsocksMethods.contains(method);
        }

        QString DisplayType() override { return "Shadowsocks"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
