#pragma once

#include "AbstractBean.hpp"
#include "Preset.hpp"
#include "V2RayStreamSettings.hpp"

namespace Configs {

    class SnellBean : public AbstractBean {
    public:
        std::shared_ptr<NetworkEnum> network = std::make_shared<NetworkEnum>(0);
        bool reuse = false;
        int snell_version = 0;
        QString psk = "";
        std::shared_ptr<ObfsModeEnum> obfs_mode = std::make_shared<ObfsModeEnum>(0);
        QString obfs_host = "";

        SnellBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
        }
        
        INIT_BEAN_MAP
            ADD_MAP("snell_version", snell_version, integer);
            ADD_MAP("reuse", reuse, boolean);
            ADD_MAP("psk", psk, string);
            ADD_MAP("obfs_mode", obfs_mode, enum);
            ADD_MAP("network", network, enum);
            ADD_MAP("obfs_host", obfs_host, string);
        STOP_MAP

        virtual QString type()const override {
            return "snell";
        };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

  //      bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

  //      QString ToShareLink() const override;
    };
} // namespace Configs
