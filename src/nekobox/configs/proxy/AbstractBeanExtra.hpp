#ifdef _WIN32
#include <winsock2.h>
#endif

#pragma once

#include <nekobox/dataStore/ProxyEntity.hpp>
#include "AbstractBean.hpp"

namespace Configs {
namespace From_CoreObj_box {

};

namespace To_Link {

};

namespace From_Link {

};

namespace To_CoreObj_box {
    template<typename T>
    void add_default_fields(T & obj, const AbstractBean * bean){
        obj["type"] = bean->type();
        obj["server"] = bean->entity->serverAddress;
        obj["server_port"] = bean->entity->serverPort;
    }

    QJsonObject getXmux(const QJsonValue & value);
    QJsonObject getXbadoptionRange(const QJsonValue & value);
    QJsonValue udp_over_tcp_object(int version);
    QJsonObject getXbadoptionRange(const QJsonValue & value);
    void parseExtraXhttp(QJsonObject & transport, QString extra);

    template<typename T>
    void add_non_empty(T & obj, const QString & key, const QString & value){
        if (!value.isEmpty()){
            obj[key] = value;
        }
    }

    template<typename T, typename B>
    void add_username_password(T & obj, B * bean){
        add_non_empty(obj, "password", bean->password);
        add_non_empty(obj, "username", bean->username);
    }

    template<typename T, typename B>
    void add_network(T & obj, B * bean){
        if (bean->network->value > 0){
            add_non_empty(obj, "network", *bean->network);
        }
    }

    template<typename T, typename B>
    void add_udp_over_tcp(T & obj, B * bean){
        obj["udp_over_tcp"] = udp_over_tcp_object(bean->uot);
    }

    template<typename T, typename B>
    void add_quic(T & obj, B * bean){
        bool quic = bean->quic;
        obj["quic"] = quic;
        if (quic){
            obj["quic_congestion_control"] = *bean->quic_congestion_control;
        }
    }

    template<typename T>
    void add_non_empty(T & obj, const QString & key, const QVariantMap & value){
        if (!value.isEmpty()){
            obj[key] = QJsonObject::fromVariantMap(value);
        }
    }
};
}