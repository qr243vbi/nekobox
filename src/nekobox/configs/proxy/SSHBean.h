#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

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

        SSHBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
        }

        INIT_BEAN_MAP
            ADD_MAP("user", user, string);
            ADD_MAP("password", password, string);
            ADD_MAP("privateKey", privateKey, string);
            ADD_MAP("privateKeyPath", privateKeyPath, string);
            ADD_MAP("privateKeyPass", privateKeyPass, string);
            ADD_MAP("hostKey", hostKey, stringList);
            ADD_MAP("hostKeyAlgs", hostKeyAlgs, stringList);
            ADD_MAP("clientVersion", clientVersion, string);
        STOP_MAP
/*/
        QString DisplayType() override { return "SSH"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        virtual QString type()const override {
            return "ssh";
        };

    };
}
