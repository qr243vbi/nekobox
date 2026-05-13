

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
     //   if (this->entity->same_path_for_bean()){
     //       return Configs::JsonStoreType::Proxies;
     //   } else {
            return Configs::JsonStoreType::Beans;
     //   }
    };

    QString AbstractBean::ToNekorayShareLink() const {
        if (this->entity == nullptr) return "";
        auto b = ToJson();
        QUrl url;
        b["name"] = this->entity->name;
        b["addr"] = this->entity->serverAddress;
        b["port"] = this->entity->serverPort;
        url.setScheme("nekoray");
        url.setHost(type());
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
                    if ((QString)*stream->network == "ws" && stream->host.isEmpty()) {
                        stream->host = domain;
                    }
                }
            }
            onFinished();
        });
    }

        bool AbstractBean::TryParseNekorayLink(const QString &str){
            auto link = QUrl(str);
            if (!link.isValid()){
                return false;
            }
            return this->TryParseNekorayLink(link);
        }

        bool AbstractBean::TryParseNekorayLink(const QUrl &link){
            if (link.host() != this->type()){
                return false;
            };
            auto j = DecodeB64IfValid(link.fragment().toUtf8(),
                              QByteArray::Base64UrlEncoding);
            if (j.isEmpty()){
                return false;
            }
            auto ret = this->FromJsonBytes(j);
            if (ret){
                auto json = QJsonDocument::fromJson(j).object();
                this->entity->name = json["name"].toString(this->entity->name);
                this->entity->serverAddress = json["addr"].toString(this->entity->serverAddress);
                this->entity->serverPort = json["port"].toInt(this->entity->serverPort);
            }
            return ret;
        }

        CoreObjOutboundBuildResult AbstractBean::BuildCoreObjSingBox() const { return {}; };

        QString AbstractBean::ToShareLink() const { return this->ToNekorayShareLink(); };

        bool AbstractBean::IsEndpoint() const { return false; };

        bool AbstractBean::TryParseLink(const QString &link) { return this->TryParseNekorayLink(link); };

        bool AbstractBean::TryParseJson(const QJsonObject &obj) { return false; };
} // namespace Configs
