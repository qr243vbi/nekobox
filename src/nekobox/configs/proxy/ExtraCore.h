#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class ExtraCoreBean : public AbstractBean {
    public:
        QString socksAddress = "127.0.0.1";
        int socksPort;
        QString extraCorePath;
        QString extraCoreArgs;
        QString extraCoreConf;
        bool noLogs;

<<<<<<< HEAD
        ExtraCoreBean() : AbstractBean(0) {
=======
        ExtraCoreBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
>>>>>>> other-repo/main
        }
        INIT_MAP
            ADD_MAP("socks_address", socksAddress, string);
            ADD_MAP("socks_port", socksPort, integer);
            ADD_MAP("extra_core_path", extraCorePath, string);
            ADD_MAP("extra_core_args", extraCoreArgs, string);
            ADD_MAP("extra_core_conf", extraCoreConf, string);
            ADD_MAP("no_logs", noLogs, boolean);
        STOP_MAP
<<<<<<< HEAD
        QString DisplayType() override { return "ExtraCore"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
=======
 //       QString DisplayType() override { return "ExtraCore"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;
        #ifdef DEBUG_MODE
        virtual QString type()const override {
            return "extra";
        };
        #endif

        QString ToShareLink() const override;
>>>>>>> other-repo/main
    };
}
