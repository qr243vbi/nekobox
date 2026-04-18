#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"

namespace Configs {

    class SocksBean : public AbstractBean {
    public:
        static constexpr int type_Socks4 = 4;
        static constexpr int type_Socks5 = 5;

        int socks_http_type = type_Socks5;
        QString username = "";
        QString password = "";
        std::shared_ptr<NetworkEnum> network = std::make_shared<NetworkEnum>(0);
        int uot = 0;


        explicit SocksBean(Configs::ProxyEntity * entity, int _socks_http_type) : AbstractBean(entity, 0) {
            this->socks_http_type = _socks_http_type;
        }
        INIT_BEAN_MAP
            ADD_MAP("socks_version", socks_http_type, integer);
            ADD_MAP("username", username, string);
            ADD_MAP("password", password, string);
            ADD_MAP("network", network, string);
            ADD_MAP("uot", uot, integer);
        STOP_MAP
/*/
        QString DisplayType() override { return socks_http_type == type_HTTP ? "HTTP" : "Socks"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox()const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink()const override;

        virtual QString type()const override {
            return "socks";
        };

    };
} // namespace Configs
