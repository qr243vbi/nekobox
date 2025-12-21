#pragma once
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <QJsonObject>
#include <QJsonArray>

#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/dataStore/Utils.hpp"


namespace Configs {
    struct CoreObjOutboundBuildResult {
    public:
        QJsonObject outbound;
        QString error;
    };

    class AbstractBean : public JsonStore {
    public:
        int version;
        virtual ConfJsMap _map() override;

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
        //

        virtual CoreObjOutboundBuildResult BuildCoreObjSingBox() { return {}; };

        virtual QString ToShareLink() { return {}; };

        virtual bool IsEndpoint() { return false; };
    };

} // namespace Configs
