#include "include/configs/proxy/includes.h"

#include <QApplication>
#include <QHostInfo>
#include <QUrl>

namespace Configs {
    AbstractBean::AbstractBean(int version) {
        this->version = version;
    }
    #undef d_add
    #define d_add(X, Y, B) _put(ptr, X, &this->Y, B)
    ConfJsMap & AbstractBean::_map(){
        static ConfJsMap ptr;
        static bool init = false;
        if (init) return ptr;
        d_add("_v", version, itemType::integer);
        d_add("name", name, itemType::string);
        d_add("addr", serverAddress, itemType::string);
        d_add("port", serverPort, itemType::integer);
        d_add("c_cfg", custom_config, itemType::string);
        d_add("c_out", custom_outbound, itemType::string);
        d_add("mux", mux_state, itemType::integer);
        d_add("enable_brutal", enable_brutal, itemType::boolean);
        d_add("brutal_speed", brutal_speed, itemType::integer);
        init = true;
        return ptr;
    }

    QString AbstractBean::ToNekorayShareLink(const QString &type) {
        auto b = ToJson();
        QUrl url;
        url.setScheme("nekoray");
        url.setHost(type);
        url.setFragment(QJsonObject2QString(b, true)
                            .toUtf8()
                            .toBase64(QByteArray::Base64UrlEncoding));
        return url.toString();
    }

    QString AbstractBean::DisplayAddress() {
        return ::DisplayAddress(serverAddress, serverPort);
    }

    QString AbstractBean::DisplayName() {
        if (name.isEmpty()) {
            return DisplayAddress();
        }
        return name;
    }

    QString AbstractBean::DisplayTypeAndName() {
        return QString("[%1] %2").arg(DisplayType(), DisplayName());
    }

    void AbstractBean::ResolveDomainToIP(const std::function<void()> &onFinished) {
        bool noResolve = false;
        if (dynamic_cast<ChainBean *>(this) != nullptr) noResolve = true;
        if (dynamic_cast<CustomBean *>(this) != nullptr) noResolve = true;
        if (IsIpAddress(serverAddress)) noResolve = true;
        if (noResolve) {
            onFinished();
            return;
        }
        QHostInfo::lookupHost(serverAddress, QApplication::instance(), [=,this](const QHostInfo &host) {
            auto addr = host.addresses();
            if (!addr.isEmpty()) {
                auto domain = serverAddress;
                auto stream = GetStreamSettings(this);

                // replace serverAddress
                serverAddress = addr.first().toString();

                // replace ws tls
                if (stream != nullptr) {
                    if (stream->security == "tls" && stream->sni.isEmpty()) {
                        stream->sni = domain;
                    }
                    if (stream->network == "ws" && stream->host.isEmpty()) {
                        stream->host = domain;
                    }
                }
            }
            onFinished();
        });
    }
} // namespace Configs
