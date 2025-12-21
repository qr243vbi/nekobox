#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"

namespace Configs {
    class SocksHttpBean : public AbstractBean {
    private:
        V2rayStreamSettings * streamPtr;
    public:
        static constexpr int type_HTTP = -80;
        static constexpr int type_Socks4 = 4;
        static constexpr int type_Socks5 = 5;

        int socks_http_type = type_Socks5;
        QString username = "";
        QString password = "";

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        explicit SocksHttpBean(int _socks_http_type) : AbstractBean(0) {
            this->socks_http_type = _socks_http_type;
            streamPtr = stream.get();
        }
        INIT_MAP
            ADD_MAP("v", socks_http_type, integer);
            ADD_MAP("username", username, string);
            ADD_MAP("password", password, string);
            ADD_MAP("stream", streamPtr, jsonStore);
        STOP_MAP

        QString DisplayType() override { return socks_http_type == type_HTTP ? "HTTP" : "Socks"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
