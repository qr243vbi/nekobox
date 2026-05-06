#ifdef _WIN32
#include <winsock2.h>
#endif

#pragma once

#include <nekobox/dataStore/ProxyEntity.hpp>
#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"

namespace Configs {
namespace From_CoreObj_box {

};

namespace To_Link {
    void add_query_int_natural(const char * name, QUrlQuery & query, int value) ;
    void add_query_int(const char * name, QUrlQuery & query, int value) ;
    void add_query_nonempty(const char * name, QUrlQuery & query, const QString &value) ;
    void add_query_args_nonempty(const char * name, QUrlQuery & query, const QStringList & value) ;
    void add_query_map_nonempty(const char * name, QUrlQuery & query, const QVariantMap & value) ;
        
    template<typename T>
    void add_network(QUrlQuery & query, T * obj){
        if (obj->network->value > 0){
            add_query_nonempty("network", query, *obj->network);
        }
    }


    void add_default_fields(QUrl & url, const AbstractBean * bean);

    void add_query_boolean(const char * name, QUrlQuery & query, bool value);

    void add_mux_state(QUrlQuery & q, const AbstractBean * bean);

    void add_query_int_range(const char * name, QUrlQuery & query, int value, int begin, int end);

    void add_default_fields(QUrl & url, const AbstractBean * bean);

    void add_mux_state(QUrlQuery & q, const AbstractBean * bean);

    void add_tls(std::shared_ptr<V2rayStreamSettings> stream, QUrlQuery & query);

    const inline char* fixShadowsocksUserNameEncodeMagic = "fixShadowsocksUserNameEncodeMagic-holder-for-QUrl";

    template<typename B>
    inline void add_quic(QUrlQuery & q, B * bean){
        add_query_nonempty("quic_congestion_control", q, (QString)*bean->quic_congestion_control);
        add_query_boolean("quic", q, bean->quic);
    }

    template<typename T>
    inline void add_udp_over_tcp(QUrlQuery & query, T * bean){
        add_query_int_range("uot", query, bean->uot, 1, 2);
    }

    template<typename B>
    inline void add_username_password(QUrl & url, B * bean){
        if (!bean->username.isEmpty()) url.setUserName(bean->username);
        if (!bean->password.isEmpty()) url.setPassword(bean->password);
    }

};

namespace From_Link {

};

namespace To_CoreObj_box {
    template<typename T>
    inline void add_default_fields(T & obj, const AbstractBean * bean){
        obj["type"] = bean->type();
        obj["server"] = bean->entity->serverAddress;
        obj["server_port"] = bean->entity->serverPort;
    }

    QJsonObject getXmux(const QJsonValue & value);
    QJsonObject getXbadoptionRange(const QJsonValue & value);
    QJsonValue udp_over_tcp_object(int version);
    QJsonObject getXbadoptionRange(const QJsonValue & value);
    int domainStrategy(const QJsonValue & value);

    void parseExtraXhttp(QJsonObject & transport, QString extra);
    void parseExtraKCP(QJsonObject & transport, std::shared_ptr<KCPExtra>  extra);

    template<typename T>
    inline void add_non_empty(T & obj, const QString & key, const QString & value){
        if (!value.isEmpty()){
            obj[key] = value;
        }
    }

    template<typename T, typename B>
    inline void add_username_password(T & obj, B * bean){
        add_non_empty(obj, "password", bean->password);
        add_non_empty(obj, "username", bean->username);
    }

    template<typename T, typename B>
    inline void add_network(T & obj, B * bean){
        if (bean->network->value > 0){
            add_non_empty(obj, "network", *bean->network);
        }
    }

    template<typename T, typename B>
    inline void add_udp_over_tcp(T & obj, B * bean){
        obj["udp_over_tcp"] = udp_over_tcp_object(bean->uot);
    }

    template<typename T, typename B>
    inline void add_quic(T & obj, B * bean){
        bool quic = bean->quic;
        obj["quic"] = quic;
        if (quic){
            obj["quic_congestion_control"] = *bean->quic_congestion_control;
        }
    }

    template<typename T>
    inline void add_non_empty(T & obj, const QString & key, const QVariantMap & value){
        if (!value.isEmpty()){
            obj[key] = QJsonObject::fromVariantMap(value);
        }
    }
};
}