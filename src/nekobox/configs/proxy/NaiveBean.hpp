#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"
#include <QVariantMap>

namespace Configs {

    class NaiveBean : public AbstractBean {
    public:
        QString username = "";
        QString password = "";
        int insecure_concurrency = 0;
        int uot = 0;
        QVariantMap extra_headers;
        bool quic = false;
        std::shared_ptr<QUICEnum> quic_congestion_control;
        std::shared_ptr<V2rayStreamSettings> stream;

        NaiveBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
            quic_congestion_control = std::make_shared<QUICEnum>("");
            stream = std::make_shared<V2rayStreamSettings>();
        }


        virtual QString type()const override {
            return "naive";
        };

        
        INIT_BEAN_MAP
            ADD_MAP("username", username, string);
            ADD_MAP("password", password, string);
            ADD_MAP("insecure_concurrency", insecure_concurrency, integer);
            ADD_MAP("uot", uot, integer);
            ADD_MAP("extra_headers", extra_headers, map);
            ADD_MAP("quic_enabled", quic, boolean);
            ADD_MAP("quic_congestion_control", quic_congestion_control, string);
            ADD_MAP("stream", stream, jsonStore);
        STOP_MAP

     //   QString DisplayType() override { return "AnyTLS"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox()const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink()const override;
    };
} // namespace Configs
