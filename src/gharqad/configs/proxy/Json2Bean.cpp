



#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>

namespace Configs
{
    void From_Json::add_default_fields(Configs::ProxyEntity * entity, const QJsonObject & obj){
        entity->name = obj["tag"].toString();
        entity->serverAddress = obj["server"].toString();
        entity->serverPort = obj["server_port"].toInt();
    }

    void From_Json::add_mux_state(AbstractBean * bean, const QJsonObject &obj){
        bean->mux_state = obj["multiplex"].isObject() ? (obj["multiplex"].toObject()["enabled"].toBool() ? 1 : 2) : 0;
    }

    bool From_Json::add_tls(std::shared_ptr<V2rayStreamSettings> stream, const QJsonObject & obj){
        bool is_tls = obj["tls"].isObject() ;
        if (is_tls) {
            QJsonObject tls = obj["tls"].toObject();
            QJsonObject reality = tls["reality"].toObject();
            auto alpn = tls["alpn"];
            auto ech_config = tls["ech_config"];
            if (!ech_config.isNull()){
                stream->enable_ech = true;
                stream->ech_config = ech_config.isArray() ? QJsonArray2QListStr(ech_config.toArray()).join("\n") : ech_config.toString();
                stream->query_server_name = tls["query_server_name"].toString();
            }
            stream->security = "tls";
            stream->reality_pbk = reality["public_key"].toString();
            stream->reality_sid = reality["short_id"].toString();
            stream->utlsFingerprint = tls["utls"].toObject()["fingerprint"].toString();
            stream->enable_tls_fragment = tls["fragment"].toBool();
            stream->tls_fragment_fallback_delay = tls["fragment_fallback_delay"].toString();
            stream->enable_tls_record_fragment = tls["record_fragment"].toBool();
            stream->sni = tls["server_name"].toString();
            stream->alpn = alpn.isArray() ? QJsonArray2QListStr(alpn.toArray()).join(",") : alpn.toString();
            stream->allow_insecure = tls["insecure"].toBool();
        } else {
            stream->security = "";
        }
        return true;
    }

    bool From_Json::parse_transport(std::shared_ptr<V2rayStreamSettings> stream, const QJsonObject & obj){
        auto transport = obj["transport"].toObject();
        QString network;
        *stream->network = network = transport["type"].toString();
        if (network == "ws" || network == "httpupgrade")
        {
            finalize:
            stream->path = transport["path"].toString();
            finalize2:
            auto host = transport["host"];
            stream->host = host.isArray() ? QJsonArray2QListStr(host.toArray()).join(",") : host.toString();
            return true;
        } else if (network == "http")
        {
            stream->method = transport["method"].toString();
            goto finalize;
        } else if (network == "grpc")
        {
            stream->path = transport["service_name"].toString();
            goto finalize2;
        } else if (network == "xhttp")
        {
            stream->xhttp_mode = transport["mode"].toString();
            goto finalize;
        }
        return false;
    }

    int From_Json::parseUOT(const QJsonObject &obj){
        int uot = 0;
        uot = obj["udp_over_tcp"].toBool();
        if (obj.contains("uot"))
        {
            QJsonValue uot_obj = obj["uot"];
            if (uot_obj.isDouble()) uot = uot_obj.toInt();
            if (uot_obj.isBool()) uot = uot_obj.toBool();
            if (uot_obj.isObject()) {
                auto uot_obj_j = uot_obj.toObject();
                uot = uot_obj_j["enabled"].toBool();
                if (uot == true){
                    auto uot_obj_v = uot_obj_j["version"];
                    if (uot_obj_v.isDouble()){
                        uot = uot_obj_v.toInt();
                    }
                }
            }
        }
        return uot;
    }
}
