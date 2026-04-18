#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"

namespace Configs {
    class HttpBean : public AbstractBean {
    public:
        QString username = "";
        QString password = "";
        QString path = "";
        QVariantMap headers;

        std::shared_ptr<V2rayStreamSettings> stream ;

        explicit HttpBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
             stream = std::make_shared<V2rayStreamSettings>();
        }
        INIT_BEAN_MAP
            ADD_MAP("username", username, string);
            ADD_MAP("password", password, string);
            ADD_MAP("stream", stream, jsonStore);
            ADD_MAP("path", path, string);
            ADD_MAP("headers", headers, stringMap);
        STOP_MAP
/*/
        QString DisplayType() override { return socks_http_type == type_HTTP ? "HTTP" : "Socks"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox()const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink()const override;

        virtual QString type()const override {
            return "http";
        };

    };
} // namespace Configs
