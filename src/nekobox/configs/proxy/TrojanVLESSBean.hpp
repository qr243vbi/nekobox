#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class TrojanVLESSBean : public AbstractBean {
    public:
        static constexpr int proxy_Trojan = 0;
        static constexpr int proxy_VLESS = 1;
        int proxy_type = proxy_Trojan;

        QString password = "";
        QString flow = "";

        std::shared_ptr<V2rayStreamSettings> stream;

        explicit TrojanVLESSBean(Configs::ProxyEntity * entity, int _proxy_type) : AbstractBean(entity, 0) {
            proxy_type = _proxy_type;
             stream = std::make_shared<V2rayStreamSettings>();
        }

        INIT_MAP
            ADD_MAP("pass", password, string);
            ADD_MAP("flow", flow, string);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP
/*/
        QString DisplayType() override { return proxy_type == proxy_VLESS ? "VLESS" : "Trojan"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;
        #ifdef DEBUG_MODE
        virtual QString type()const override {
             return proxy_type == proxy_VLESS ? "vless" : "trojan";
        };
        #endif
    };
} // namespace Configs
