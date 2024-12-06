#pragma once

#include <QJsonObject>
#include <QJsonArray>

#include "include/global/NekoGui.hpp"

namespace NekoGui_fmt {
    struct CoreObjOutboundBuildResult {
    public:
        QJsonObject outbound;
        QString error;
    };

    struct ExternalBuildResult {
    public:
        QString program;
        QStringList env;
        QStringList arguments;
        //
        QString tag;
        //
        QString error;
        QString config_export;
    };

    class AbstractBean : public JsonStore {
    public:
        int version;

        QString name = "";
        QString serverAddress = "127.0.0.1";
        int serverPort = 1080;

        QString custom_config = "";
        QString custom_outbound = "";
        int mux_state = 0;
        bool enable_brutal = false;
        int brutal_speed = 0;

        explicit AbstractBean(int version);

        //

        QString ToNekorayShareLink(const QString &type);

        void ResolveDomainToIP(const std::function<void()> &onFinished);

        //

        [[nodiscard]] virtual QString DisplayAddress();

        [[nodiscard]] virtual QString DisplayName();

        virtual QString DisplayCoreType() { return software_core_name; };

        virtual QString DisplayType() { return {}; };

        virtual QString DisplayTypeAndName();

        virtual bool IsValid();

        //

        virtual int NeedExternal(bool isFirstProfile) { return 0; };

        virtual CoreObjOutboundBuildResult BuildCoreObjSingBox() { return {}; };

        virtual ExternalBuildResult BuildExternal(int mapping_port, int socks_port, int external_stat) { return {}; };

        virtual QString ToShareLink() { return {}; };
    };

} // namespace NekoGui_fmt
