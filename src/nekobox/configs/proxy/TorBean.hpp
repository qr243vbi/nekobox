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

        TorBean() : AbstractBean(0) {
        }
        
        INIT_MAP
            ADD_MAP("executable_path", executable_path, string);
            ADD_MAP("extra_args", extra_args, stringList);
            ADD_MAP("data_directory", data_directory, string);
            ADD_MAP("torrc", torrc, stringMap);
        STOP_MAP

        QString DisplayType() override { return "Tor"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
