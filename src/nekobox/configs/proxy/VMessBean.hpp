#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class VMessBean : public AbstractBean {
    public:
        QString uuid = "";
        int aid = 0;
        QString security = "auto";
        bool authenticated_length = false;
        bool global_padding = true;

        std::shared_ptr<V2rayStreamSettings> stream ;
        std::shared_ptr<NetworkEnum> network = std::make_shared<NetworkEnum>(0);

        VMessBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
             stream = std::make_shared<V2rayStreamSettings>();
        }

        INIT_BEAN_MAP
            ADD_MAP("id", uuid, string);
            ADD_MAP("aid", aid, integer);
            ADD_MAP("sec", security, string);
            ADD_MAP("stream", stream, jsonStore);
            ADD_MAP("network", network, string);
            ADD_MAP("authenticated_length", authenticated_length, boolean);
            ADD_MAP("global_padding", global_padding, boolean);
        STOP_MAP
/*/
        QString DisplayType() override { return "VMess"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        virtual QString type()const override {
            return "vmess";
        };

    };
} // namespace Configs
