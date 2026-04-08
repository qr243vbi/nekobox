#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <QApplication>
#include <QHostInfo>
#include <QUrl>

namespace Configs {
    AbstractBean::AbstractBean(Configs::ProxyEntity * entity, int version) {
        this->entity = entity;
        this->version = version;
    }
    DECL_MAP(AbstractBean)
        ADD_MAP("_v", version, integer);
        ADD_MAP("c_cfg", custom_config, string);
        ADD_MAP("c_out", custom_outbound, string);
        ADD_MAP("mux", mux_state, integer);
        ADD_MAP("enable_brutal", enable_brutal, boolean);
        ADD_MAP("brutal_speed", brutal_speed, integer);
    STOP_MAP

    char AbstractBean::StoreType() const {
        if (this->entity->same_path_for_bean()){
            return Configs::JsonStoreType::Proxies;
        } else {
            return Configs::JsonStoreType::Beans;
        }
    };

    QString AbstractBean::ToNekorayShareLink(const QString &type) const {
        if (this->entity == nullptr) return "";
        auto b = ToJson();
        QUrl url;
        url.setScheme("nekoray");
        url.setHost(type);
        url.setFragment(QJsonObject2QString(b, true)
                            .toUtf8()
                            .toBase64(QByteArray::Base64UrlEncoding));
        return url.toString();
    }

    int AbstractBean::Id() const {
        return this->entity->id;
    }
    
    QString AbstractBean::type()const {
        return "unknown";
    }

    bool AbstractBean::UnknownKeyHash(const QByteArray & array) {
        if (array == bean_key){
            return true;
        }
        return false;
    };

    AbstractBean::~AbstractBean() {
        bool save_control_no_save = this->save_control_no_save();
        #ifdef DEBUG_MODE
            qDebug() << "DO NOT SAVE BEFORE BLOW" << save_control_no_save;
        #endif
        if (!save_control_no_save && (this->entity != nullptr)){
            this->entity->Save();
        }
    }

    void AbstractBean::ResolveDomainToIP(const std::function<void()> &onFinished) {
        if (this->entity == nullptr) return;
        bool noResolve = false;
        auto serverAddress = entity->serverAddress;
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
                this->entity->serverAddress = addr.first().toString();

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
