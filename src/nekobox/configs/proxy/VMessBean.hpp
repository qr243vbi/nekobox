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

<<<<<<< HEAD
        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        VMessBean() : AbstractBean(0) {
=======
        std::shared_ptr<V2rayStreamSettings> stream ;

        VMessBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
             stream = std::make_shared<V2rayStreamSettings>();
>>>>>>> other-repo/main
        }

        INIT_MAP
            ADD_MAP("id", uuid, string);
            ADD_MAP("aid", aid, integer);
            ADD_MAP("sec", security, string);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP
<<<<<<< HEAD

        QString DisplayType() override { return "VMess"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
=======
/*/
        QString DisplayType() override { return "VMess"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;
        #ifdef DEBUG_MODE
        virtual QString type()const override {
            return "vmess";
        };
        #endif
>>>>>>> other-repo/main
    };
} // namespace Configs
