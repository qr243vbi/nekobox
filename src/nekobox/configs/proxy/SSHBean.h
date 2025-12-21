#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class SSHBean : public AbstractBean {
    public:
        QString user = "root";
        QString password;
        QString privateKey;
        QString privateKeyPath;
        QString privateKeyPass;
        QStringList hostKey;
        QStringList hostKeyAlgs;
        QString clientVersion;

        SSHBean() : AbstractBean(0) {
        }

        INIT_MAP
            ADD_MAP("user", user, string);
            ADD_MAP("password", password, string);
            ADD_MAP("privateKey", privateKey, string);
            ADD_MAP("privateKeyPath", privateKeyPath, string);
            ADD_MAP("privateKeyPass", privateKeyPass, string);
            ADD_MAP("hostKey", hostKey, stringList);
            ADD_MAP("hostKeyAlgs", hostKeyAlgs, stringList);
            ADD_MAP("clientVersion", clientVersion, string);
        STOP_MAP

        QString DisplayType() override { return "SSH"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
}
