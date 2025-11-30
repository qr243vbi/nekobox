#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class TrojanVLESSBean : public AbstractBean {
    private:
        V2rayStreamSettings * streamPtr;
    public:
        static constexpr int proxy_Trojan = 0;
        static constexpr int proxy_VLESS = 1;
        int proxy_type = proxy_Trojan;

        QString password = "";
        QString flow = "";

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        explicit TrojanVLESSBean(int _proxy_type) : AbstractBean(0) {
            proxy_type = _proxy_type;
            streamPtr = stream.get();
        }
#define _add(X, Y, B) ADD_MAP(X, Y, B)

        INIT_MAP
            _add("pass", password, string);
            _add("flow", flow, string);
            _add("stream", streamPtr, jsonStore);
        STOP_MAP

        QString DisplayType() override { return proxy_type == proxy_VLESS ? "VLESS" : "Trojan"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs