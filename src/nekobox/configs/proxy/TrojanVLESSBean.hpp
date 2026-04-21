#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

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
        std::shared_ptr<NetworkEnum> network = std::make_shared<NetworkEnum>(0);

        QString password = "";
        QString flow = "";
        QString encryption = "";

        std::shared_ptr<V2rayStreamSettings> stream;

        explicit TrojanVLESSBean(Configs::ProxyEntity * entity, int _proxy_type) : AbstractBean(entity, 0) {
            proxy_type = _proxy_type;
             stream = std::make_shared<V2rayStreamSettings>();
        }

        INIT_BEAN_MAP
            ADD_MAP("network", network, string);
            ADD_MAP("pass", password, string);
            ADD_MAP("flow", flow, string);
            ADD_MAP("stream", stream, jsonStore);
            ADD_MAP("enc", encryption, string);
        STOP_MAP
/*/
        QString DisplayType() override { return proxy_type == proxy_VLESS ? "VLESS" : "Trojan"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        virtual QString type()const override {
             return proxy_type == proxy_VLESS ? "vless" : "trojan";
        };

    };
} // namespace Configs
