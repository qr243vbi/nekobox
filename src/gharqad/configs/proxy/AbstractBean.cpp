#include <qnamespace.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <QThreadPool>
#include <QMetaObject>
#include <QPointer>


#include <QApplication>
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
void AbstractBean::ResolveDomainToIP(const std::function<void()> &onFinished)
{
    if (entity == nullptr)
        return;

    auto serverAddress = entity->serverAddress;

    bool noResolve =
        dynamic_cast<ChainBean*>(this) != nullptr ||
        dynamic_cast<CustomBean*>(this) != nullptr ||
        IsIpAddress(serverAddress);

    if (noResolve)
    {
        onFinished();
        return;
    }

    QThreadPool::globalInstance()->start(
        [this, serverAddress, onFinished]
        {
            QString ip;

            addrinfo hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            addrinfo* result = nullptr;

            if (getaddrinfo(
                    serverAddress.toUtf8().constData(),
                    nullptr,
                    &hints,
                    &result) == 0)
            {
                char buffer[INET6_ADDRSTRLEN];

                for (auto* p = result; p; p = p->ai_next)
                {
                    if (p->ai_family == AF_INET)
                    {
                        auto* addr = (sockaddr_in*)p->ai_addr;
                        inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer));
                        ip = buffer;
                        break;
                    }
                    else if (p->ai_family == AF_INET6)
                    {
                        auto* addr = (sockaddr_in6*)p->ai_addr;
                        inet_ntop(AF_INET6, &addr->sin6_addr, buffer, sizeof(buffer));
                        ip = buffer;
                        break;
                    }
                }

                freeaddrinfo(result);
            }

    //        QMetaObject::invokeMethod(
      //          qApp,
        //        [this, serverAddress, ip, onFinished]
                {
                    if (!ip.isEmpty())
                    {
                        auto stream = GetStreamSettings(this);

                        entity->serverAddress = ip;

                        if (stream)
                        {
                            if (stream->security.compare("tls", Qt::CaseInsensitive) == 0 &&
                                stream->sni.isEmpty())
                            {
                                stream->sni = serverAddress;
                            }

                            if (stream->network->toString().compare("ws", Qt::CaseInsensitive) == 0 &&
                                stream->host.isEmpty())
                            {
                                stream->host = serverAddress;
                            }
                        }
                    }

                    onFinished();
                }//);
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
