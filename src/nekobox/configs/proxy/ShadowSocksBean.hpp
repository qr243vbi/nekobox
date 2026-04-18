#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class ShadowSocksBean : public AbstractBean {
    public:
        QString method = "aes-128-gcm";
        QString password = "";
        QString plugin = "";
        QString plugin_opts = "";
        std::shared_ptr<NetworkEnum> network = std::make_shared<NetworkEnum>(0);
        int uot = 0;

   //     std::shared_ptr<V2rayStreamSettings> stream;

        ShadowSocksBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
   //         stream = std::make_shared<V2rayStreamSettings>();
        }

        INIT_BEAN_MAP
            ADD_MAP("method", method, string);
            ADD_MAP("pass", password, string);
            ADD_MAP("plugin", plugin, string);
            ADD_MAP("plugin_opts", plugin_opts, string);
            ADD_MAP("uot", uot, integer);
   //         ADD_MAP("stream", stream, jsonStore);
            ADD_MAP("network", network, string);
        STOP_MAP

        bool IsValid() {
            return Preset::SingBox::ShadowsocksMethods.contains(method);
        }
/*/
        QString DisplayType() override { return "Shadowsocks"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        bool TryParseFromSIP008(const QJsonObject& object);

        QString ToShareLink() const override;

        virtual QString type()const override {
            return "shadowsocks";
        };

    };
} // namespace Configs
