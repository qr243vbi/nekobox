#pragma once

#include <QJsonObject>
#include <QJsonArray>

#include "include/global/Configs.hpp"


#ifndef ADD_MAP


#define INIT_MAP_1 virtual ConfJsMap & _map() override {   \
static ConfJsMap ptr;                   \
static bool init = false;               \
if (init) return ptr;                    

#define INIT_MAP INIT_MAP_1              \
ptr = AbstractBean::_map();   

#define STOP_MAP \
init = true;     \
return ptr;      \
}

#define ADD_MAP(X, Y, B) _put(ptr, X, &this->Y, itemType::B)
#define _add(X, Y, B) ADD_MAP(X, Y, B)
#endif
#ifndef _add
#define _add(X, Y, B) ADD_MAP(X, Y, B)
#endif

namespace Configs {
    struct CoreObjOutboundBuildResult {
    public:
        QJsonObject outbound;
        QString error;
    };

    class AbstractBean : public JsonStore {
    public:
        int version;
        virtual ConfJsMap& _map() override;

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
