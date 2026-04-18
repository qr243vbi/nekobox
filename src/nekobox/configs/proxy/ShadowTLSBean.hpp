#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class ShadowTLSBean : public AbstractBean {
    public:
        QString password = "";
        int shadowtls_version = 1;

        std::shared_ptr<V2rayStreamSettings> stream ;

        ShadowTLSBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
             stream = std::make_shared<V2rayStreamSettings>();
        }
        
        INIT_BEAN_MAP
            ADD_MAP("password", password, string);
            ADD_MAP("shadowtls_version", shadowtls_version, integer);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP
/*/
        QString DisplayType() override { return "ShadowTLS"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link)  override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        virtual QString type()const override {
            return "shadowtls";
        };

    };
} // namespace Configs
