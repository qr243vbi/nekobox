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
#define _add(X, Y, B) ADD_MAP(X, Y, B)

        INIT_MAP
            _add("user", user, string);
            _add("password", password, string);
            _add("privateKey", privateKey, string);
            _add("privateKeyPath", privateKeyPath, string);
            _add("privateKeyPass", privateKeyPass, string);
            _add("hostKey", hostKey, stringList);
            _add("hostKeyAlgs", hostKeyAlgs, stringList);
            _add("clientVersion", clientVersion, string);
        STOP_MAP
        #undef _add

        QString DisplayType() override { return "SSH"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
}
