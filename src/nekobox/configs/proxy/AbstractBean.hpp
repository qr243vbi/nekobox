#pragma once

#include <QJsonObject>
#include <QJsonArray>

#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/dataStore/Utils.hpp"


namespace Configs {
<<<<<<< HEAD
=======
    extern QByteArray bean_key;

>>>>>>> other-repo/main
    struct CoreObjOutboundBuildResult {
    public:
        QJsonObject outbound;
        QString error;
    };

<<<<<<< HEAD
=======
    class ProxyEntity;

>>>>>>> other-repo/main
    class AbstractBean : public JsonStore {
    public:
        int version;
        virtual ConfJsMap _map() override;

<<<<<<< HEAD
        QString name = "";
        QString serverAddress = "127.0.0.1";
        int serverPort = 1080;

=======
>>>>>>> other-repo/main
        QString custom_config = "";
        QString custom_outbound = "";
        int mux_state = 0;
        bool enable_brutal = false;
        int brutal_speed = 0;
<<<<<<< HEAD

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
=======
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
        virtual QString type() const;
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

        virtual bool TryParseLink(const QString &link) { return false; };

        virtual bool TryParseJson(const QJsonObject &obj) { return false; };
>>>>>>> other-repo/main
    };

} // namespace Configs
