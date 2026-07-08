



#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>

namespace Configs
{
    void From_Json::add_default_fields(Configs::ProxyEntity * entity, const Data::Node & obj){
        entity->name = obj["tag"].toString();
        entity->serverAddress = obj["server"].toString();
        entity->serverPort = obj["server_port"].toInt();
    }

    void From_Json::add_mux_state(AbstractBean * bean, const Data::Node &obj){
        bean->mux_state = obj["multiplex"].isObject() ? (obj["multiplex"]["enabled"].toBool() ? 1 : 2) : 0;
    }

    bool From_Json::add_tls(std::shared_ptr<V2rayStreamSettings> stream, const Data::Node & obj){
        bool is_tls = obj["tls"].isObject() ;
        if (is_tls) {
            auto &tls = obj["tls"];
            auto &reality = tls["reality"];
            auto &alpn = tls["alpn"];
            auto &ech_config = tls["ech_config"];
            if (!ech_config.isNothing()){
                stream->enable_ech = true;
                stream->ech_config = ech_config.isArray() ? ech_config.toStringList().join("\n") : ech_config.toString();
                stream->query_server_name = tls["query_server_name"].toString();
            }
            stream->security = "tls";
            stream->reality_pbk = reality["public_key"].toString();
            stream->reality_sid = reality["short_id"].toString();
            stream->utlsFingerprint = tls["utls"]["fingerprint"].toString();
            stream->enable_tls_fragment = tls["fragment"].toBool();
            stream->tls_fragment_fallback_delay = tls["fragment_fallback_delay"].toString();
            stream->enable_tls_record_fragment = tls["record_fragment"].toBool();
            stream->sni = tls["server_name"].toString();
            stream->alpn = alpn.isArray() ? alpn.toStringList().join(",") : alpn.toString();
            stream->allow_insecure = tls["insecure"].toBool();
        } else {
            stream->security = "";
        }
        return true;
    }

    bool From_Json::parse_transport(std::shared_ptr<V2rayStreamSettings> stream, const Data::Node & obj){
        auto &transport = obj["transport"];
        QString network;
        *stream->network = network = transport["type"].toString();
        if (network == "ws" || network == "httpupgrade")
        {
            finalize:
            stream->path = transport["path"].toString();
            finalize2:
            auto &host = transport["host"];
            stream->host = host.isArray() ? host.toStringList().join(",") : host.toString();
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

    int From_Json::parseUOT(const Data::Node &obj){
        int uot = 0;
        uot = obj["udp_over_tcp"].toBool();
        auto &uot_obj = obj["uot"];
        if (!uot_obj.isNothing()){
            if (uot_obj.isDouble()) uot = uot_obj.toInt();
            if (uot_obj.isBool()) uot = uot_obj.toBool();
            if (uot_obj.isObject()) {
                auto &uot_obj_j = uot_obj;
                uot = uot_obj_j["enabled"].toBool();
                if (uot == true){
                    auto &uot_obj_v = uot_obj_j["version"];
                    if (uot_obj_v.isDouble()){
                        uot = uot_obj_v.toInt();
                    }
                }
            }
        }
        return uot;
    }
}
