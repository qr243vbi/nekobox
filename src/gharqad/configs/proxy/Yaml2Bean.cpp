



#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>

namespace Configs
{
    int From_Yaml::parseUOT(const Data::Node &obj){
        int uot = 0;
        uot = obj["udp-over-tcp"].toBool();
        auto &uot_obj = obj["udp-over-tcp-version"];
        if (uot_obj.isNumber()){
            uot = uot_obj.toNumber();
        }
        return uot;
    }

    bool From_Yaml::parse_transport(std::shared_ptr<V2rayStreamSettings> stream, const Data::Node & obj){
        return From_Json::parse_transport(true, stream, obj);
    }

    void From_Yaml::add_mux_state(AbstractBean * bean, const Data::Node &obj){
        bean->mux_state = obj["smux"].isObject() ? (obj["smux"]["enabled"].toBool() ? 1 : 2) : 0;
    }

    void From_Yaml::add_default_fields(Configs::ProxyEntity * entity, const Data::Node & obj){
        entity->name = obj["name"].toString();
        entity->serverAddress = obj["server"].toString();
        entity->serverPort = obj["port"].toInt();
    }

    bool From_Yaml::add_tls(std::shared_ptr<V2rayStreamSettings> stream, const Data::Node & tls){
        bool is_tls = tls["tls"].toBool() ;
        if (is_tls) {
            auto &reality = tls["reality-opts"];
            auto &alpn = tls["alpn"];
            auto &ech_config = tls["ech-opts"];
            if (!ech_config.isNothing()){
                stream->ech_config = DecodeB64IfValid(tls["config"].toString());
                stream->enable_ech = !stream->ech_config.isEmpty();
                stream->query_server_name = tls["query-server-name"].toString();
            }
            stream->security = "tls";
            stream->reality_pbk = reality["public-key"].toString();
            stream->reality_sid = reality["short-id"].toString();
            stream->utlsFingerprint = tls["client-fingerprint"].toString();
       //     stream->enable_tls_fragment = tls["fragment"].toBool();
       //     stream->tls_fragment_fallback_delay = tls["fragment_fallback_delay"].toString();
       //     stream->enable_tls_record_fragment = tls["record_fragment"].toBool();
            stream->sni = tls["sni"].toString();
            stream->alpn = alpn.isArray() ? alpn.toStringList().join(",") : alpn.toString();
            stream->allow_insecure = tls["insecure"].toBool();
        } else {
            stream->security = "";
        }
        return true;
    }
}
