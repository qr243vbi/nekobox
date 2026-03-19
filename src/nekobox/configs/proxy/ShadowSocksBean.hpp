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
        int uot = 0;

<<<<<<< HEAD
        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        ShadowSocksBean() : AbstractBean(0) {
=======
        std::shared_ptr<V2rayStreamSettings> stream;

        ShadowSocksBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
            stream = std::make_shared<V2rayStreamSettings>();
>>>>>>> other-repo/main
        }

        INIT_MAP
            ADD_MAP("method", method, string);
            ADD_MAP("pass", password, string);
            ADD_MAP("plugin", plugin, string);
            ADD_MAP("plugin_opts", plugin_opts, string);
            ADD_MAP("uot", uot, integer);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP

        bool IsValid() {
            return Preset::SingBox::ShadowsocksMethods.contains(method);
        }
<<<<<<< HEAD

        QString DisplayType() override { return "Shadowsocks"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        bool TryParseFromSIP008(const QJsonObject& object);

        QString ToShareLink() override;
=======
/*/
        QString DisplayType() override { return "Shadowsocks"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        bool TryParseFromSIP008(const QJsonObject& object);

        QString ToShareLink() const override;
        #ifdef DEBUG_MODE
        virtual QString type()const override {
            return "shadowsocks";
        };
        #endif
>>>>>>> other-repo/main
    };
} // namespace Configs
