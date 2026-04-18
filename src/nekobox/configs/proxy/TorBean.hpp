#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class TorBean : public AbstractBean {
    public:
        QString executable_path;
        QStringList extra_args;
        QString data_directory;
        QVariantMap torrc;

        TorBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
        }
        
        INIT_BEAN_MAP
            ADD_MAP("executable_path", executable_path, string);
            ADD_MAP("extra_args", extra_args, stringList);
            ADD_MAP("data_directory", data_directory, string);
            ADD_MAP("torrc", torrc, stringMap);
        STOP_MAP
/*/
        QString DisplayType() override { return "Tor"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        virtual QString type()const override {
            return "tor";
        };

    };
} // namespace Configs
