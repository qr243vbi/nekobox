#pragma once

#include <QJsonObject>
#include <QJsonArray>

#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/dataStore/Utils.hpp"


namespace Configs {
    extern QByteArray bean_key;

    struct CoreObjOutboundBuildResult {
    public:
        QJsonObject outbound;
        QString error;
    };

    class ProxyEntity;

    class AbstractBean : public JsonStore {
    public:
        int version;
        virtual ConfJsMap _map() override;

        QString custom_config = "";
        QString custom_outbound = "";
        int mux_state = 0;
        bool enable_brutal = false;
        int brutal_speed = 0;
        Configs::ProxyEntity * entity = nullptr;

        ~AbstractBean() override;
   //     virtual bool Save() override;

        explicit AbstractBean(Configs::ProxyEntity * entity, int version);
        //

        QString ToNekorayShareLink(const QString &type) const;

        void ResolveDomainToIP(const std::function<void()> &onFinished);
        virtual bool UnknownKeyHash(const QByteArray & array) override;

        //
        #ifdef DEBUG_MODE
        virtual QString type() ;
        #endif
//        [[nodiscard]] virtual QString DisplayAddress();

//        [[nodiscard]] virtual QString DisplayName();

//        virtual QString DisplayCoreType() { return software_core_name; };

//        virtual QString DisplayType() { return {}; };

//        virtual QString DisplayTypeAndName();
        //
        

        virtual CoreObjOutboundBuildResult BuildCoreObjSingBox() const { return {}; };

        virtual QString ToShareLink() const { return {}; };

        virtual bool IsEndpoint() const { return false; };
    };

} // namespace Configs
