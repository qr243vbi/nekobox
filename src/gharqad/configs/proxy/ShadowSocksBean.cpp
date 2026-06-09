



#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {


    CoreObjOutboundBuildResult ShadowSocksBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        QJsonObject outbound;
        add_default_fields(outbound, this);
        outbound["method"] = method;
        outbound["password"] = password;
        add_network(outbound, this);
        add_udp_over_tcp(outbound, this);

        if (!plugin.trimmed().isEmpty()) {
            outbound["plugin"] = SubStrBefore(plugin, ";");
            outbound["plugin_opts"] = SubStrAfter(plugin, ";");
        }

   //     stream->BuildStreamSettingsSingBox(&outbound);
        result.outbound = outbound;
        return result;
    }

    bool ShadowSocksBean::TryParseFromSIP008(const QJsonObject& object){
        if (object.isEmpty()) return false;
        TryParseJson(object);
        if (object.contains("remarks")) entity->name = object["remarks"].toString();
        return !( entity->serverAddress.isEmpty() || method.isEmpty() || password.isEmpty());
    }
    bool ShadowSocksBean::TryParseJson(const QJsonObject& obj)
    {    
        using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
//        method = obj["method"].toString();
//        password = obj["password"].toString();
//        plugin = obj["plugin"].toString();
        add_network(this, obj);
        add_udp_over_tcp(this, obj);
        if (obj.contains("method")) method = obj["method"].toString();
        if (obj.contains("password")) password = obj["password"].toString();
        if (obj.contains("plugin")) plugin = obj["plugin"].toString();
        if (obj.contains("plugin_opts")) plugin_opts = obj["plugin_opts"].toString();
        add_mux_state(this, obj);
        return true;
    }
    bool ShadowSocksBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        if (SubStrBefore(link, "#").contains("@")) {
            // SS
            auto url = QUrl(link);
            if (!url.isValid()) return false;
            add_default_fields(url, entity);

            if (url.password().isEmpty()) {
                // traditional format
                auto method_password = DecodeB64IfValid(url.userName(), QByteArray::Base64Option::Base64UrlEncoding);
                if (method_password.isEmpty()) return false;
                method = SubStrBefore(method_password, ":");
                password = SubStrAfter(method_password, ":");
            } else {
                // 2022 format
                method = url.userName();
                password = url.password();
            }

            auto query = GetQuery(url);
            plugin = query.queryItemValue("plugin").replace("simple-obfs;", "obfs-local;");
            
            add_mux_state(this, query);
            add_network(this, query);
            add_udp_over_tcp(this, query);

        } else {
            // v2rayN
            DECODE_V2RAY_N_1

            if (hasRemarks) entity->name = url.fragment(QUrl::FullyDecoded);
            entity->serverAddress = url.host();
            entity->serverPort = url.port();
            method = url.userName();
            password = url.password();
        }

        return !(entity->serverAddress.isEmpty() || method.isEmpty() || password.isEmpty());
    }

    QString ShadowSocksBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("ss");
        if (method.startsWith("2022-")) {
            url.setUserName(fixShadowsocksUserNameEncodeMagic);
        } else {
            auto method_password = method + ":" + password;
            url.setUserName(method_password.toUtf8().toBase64(QByteArray::Base64Option::Base64UrlEncoding));
        }
        add_default_fields(url, this);
        QUrlQuery query;
        add_query_nonempty("plugin", query, plugin);

        // mux
        add_mux_state(query, this);
        // uot
        add_udp_over_tcp(query, this);

        add_network(query, this);
        if (!query.isEmpty()) url.setQuery(query);
        //
        auto link = url.toString(QUrl::FullyEncoded);
        link = link.replace(fixShadowsocksUserNameEncodeMagic, method + ":" + QUrl::toPercentEncoding(password));
        return link;
    }

}