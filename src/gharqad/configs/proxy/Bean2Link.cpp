

#include <nekobox/configs/proxy/V2RayStreamSettings.hpp>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>

#include <QUrlQuery>
#include <QProcess>
#include <qurlquery.h>

namespace Configs {
namespace To_Link {
    void add_query_int_natural(const char * name, QUrlQuery & query, int value){ AddQueryNatural(query, name, value); };
    void add_query_int(const char * name, QUrlQuery & query, int value){ AddQueryInt(query, name, value); };
    void add_query_nonempty(const char * name, QUrlQuery & query, const QString &value){ AddQueryString(query, name, value); };
    void add_query_args_nonempty(const char * name, QUrlQuery & query, const QStringList & value) { AddQueryStringList(query, name, value); };
    void add_query_map_nonempty(const char * name, QUrlQuery & query, const QVariantMap & value) { AddQueryMap(query, name, value); };

    void add_default_fields(QUrl & url, const AbstractBean * bean){
        auto name = bean->entity->name;
        if (!name.isEmpty()) url.setFragment(name);
        url.setHost(bean->entity->serverAddress);
        url.setPort(bean->entity->serverPort);
    }

    void add_query_boolean(const char * name, QUrlQuery & query, bool value){
        add_query_nonempty(name, query, value ? "true" : "false");
    };

    void add_mux_state(QUrlQuery & q, const AbstractBean * bean){
        if (bean->mux_state != 0) {
            add_query_boolean("mux", q, (bean->mux_state == 1) ? true : false);
        }
    }

    void add_query_int_range(const char * name, QUrlQuery & query, int value, int begin, int end){
        if (value >= begin && value <= end){
            add_query_int(name, query, value);
        }
    }

    void add_tls(std::shared_ptr<V2rayStreamSettings> stream, QUrlQuery & query){
        auto security = stream->security;
        if (security == "tls" && !stream->reality_pbk.trimmed().isEmpty()) security = "reality";
        query.addQueryItem("security", security == "" ? "none" : security);

        add_query_nonempty("sni", query, stream->sni);
        add_query_nonempty("alpn", query, stream->alpn);
        if (stream->enable_ech){
            add_query_nonempty("ech_config", query, stream->ech_config);
            add_query_nonempty("query_server_name", query, stream->query_server_name);
        }
        if (stream->allow_insecure) query.addQueryItem("insecure", "1");
        if (stream->utlsFingerprint.isEmpty()) {
            query.addQueryItem("fp", Configs::dataStore->utlsFingerprint);
        } else {
            query.addQueryItem("fp", stream->utlsFingerprint);
        }
        if (stream->enable_tls_fragment) query.addQueryItem("fragment", "1");
        add_query_nonempty("fragment_fallback_delay", query, stream->tls_fragment_fallback_delay);
        if (stream->enable_tls_record_fragment) query.addQueryItem("record_fragment", "1");

        if (security == "reality") {
            add_query_nonempty("pbk", query, stream->reality_pbk);
            add_query_nonempty("sid", query, stream->reality_sid);
        }
        QString network = (QString)*stream->network;

        // type
        add_query_nonempty("type", query, network);

        if (network == "ws" || network == "httpupgrade") {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
        } else if (network == "http" ) {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
            add_query_nonempty("method", query, stream->method);
        } else if (network == "grpc") {
            add_query_nonempty("serviceName", query, stream->path);
        } else if (network == "tcp") {
            if (stream->header_type == "http") {
                add_query_nonempty("path", query, stream->path);
                add_query_nonempty("headerType", query, "http");
                add_query_nonempty("host", query, stream->host);
            }
        } else if (network == "xhttp") {
            add_query_nonempty("path", query, stream->path);
            add_query_nonempty("host", query, stream->host);
            add_query_nonempty("mode", query, stream->xhttp_mode);
            add_query_nonempty("extra", query, stream->xhttp_extra);
        }

    }
}

} // namespace Configs
