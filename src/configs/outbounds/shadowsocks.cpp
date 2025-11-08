#include "include/configs/outbounds/shadowsocks.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool shadowsocks::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        commons->ParseFromLink(link);

        if (SubStrBefore(link, "#").contains("@")) {
            // Traditional SS format
            if (url.password().isEmpty()) {
                // Traditional format: method:password base64 encoded in username
                auto method_password = DecodeB64IfValid(url.userName(), QByteArray::Base64Option::Base64UrlEncoding);
                if (method_password.isEmpty()) return false;
                method = SubStrBefore(method_password, ":");
                password = SubStrAfter(method_password, ":");
            } else {
                // 2022 format: method in username, password in password
                method = url.userName();
                password = url.password();
            }
        } else {
            // v2rayN format: base64 encoded full URL
            QString linkN = DecodeB64IfValid(SubStrBefore(SubStrAfter(link, "://"), "#"), QByteArray::Base64Option::Base64UrlEncoding);
            if (linkN.isEmpty()) return false;
            if (link.contains("#")) linkN += "#" + SubStrAfter(link, "#");
            url = QUrl("https://" + linkN);
            if (!url.isValid()) return false;
            query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));
            commons->ParseFromLink(url.toString());
            method = url.userName();
            password = url.password();
        }

        plugin = query.queryItemValue("plugin").replace("simple-obfs;", "obfs-local;");
        if (query.hasQueryItem("plugin-opts")) plugin_opts = query.queryItemValue("plugin-opts");
        if (query.hasQueryItem("uot")) uot = query.queryItemValue("uot") == "true" || query.queryItemValue("uot").toInt() > 0;
        multiplex->ParseFromLink(link);

        return !(commons->server.isEmpty() || method.isEmpty() || password.isEmpty());
    }

    bool shadowsocks::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "shadowsocks") return false;
        commons->ParseFromJson(object);
        if (object.contains("method")) method = object["method"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("plugin")) plugin = object["plugin"].toString();
        if (object.contains("plugin_opts")) plugin_opts = object["plugin_opts"].toString();
        if (object.contains("uot"))
        {
            if (object["uot"].isBool()) uot = object["uot"].toBool();
            if (object["uot"].isObject()) uot = object["uot"].toObject()["enabled"].toBool();
        }
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return true;
    }

    QString shadowsocks::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("ss");
        
        if (method.startsWith("2022-")) {
            // 2022 format: method:password directly
            url.setUserName(method);
            url.setPassword(password);
        } else {
            // Traditional format: base64 encode method:password
            auto method_password = method + ":" + password;
            url.setUserName(method_password.toUtf8().toBase64(QByteArray::Base64Option::Base64UrlEncoding));
        }
        
        url.setHost(commons->server);
        url.setPort(commons->server_port);
        if (!commons->name.isEmpty()) url.setFragment(commons->name);
        
        if (!plugin.isEmpty()) query.addQueryItem("plugin", plugin);
        if (!plugin_opts.isEmpty()) query.addQueryItem("plugin-opts", plugin_opts);
        if (uot) query.addQueryItem("uot", "true");
        
        mergeUrlQuery(query, multiplex->ExportToLink());
        mergeUrlQuery(query, commons->ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString();
    }

    QJsonObject shadowsocks::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "shadowsocks";
        mergeJsonObjects(object, commons->ExportToJson());
        if (!method.isEmpty()) object["method"] = method;
        if (!password.isEmpty()) object["password"] = password;
        if (!plugin.isEmpty()) object["plugin"] = plugin;
        if (!plugin_opts.isEmpty()) object["plugin_opts"] = plugin_opts;
        if (uot) object["uot"] = uot;
        if (multiplex->enabled) object["multiplex"] = multiplex->ExportToJson();
        return object;
    }

    BuildResult shadowsocks::Build()
    {
        QJsonObject object;
        object["type"] = "shadowsocks";
        mergeJsonObjects(object, commons->Build().object);
        if (!method.isEmpty()) object["method"] = method;
        if (!password.isEmpty()) object["password"] = password;
        if (!plugin.isEmpty()) object["plugin"] = plugin;
        if (!plugin_opts.isEmpty()) object["plugin_opts"] = plugin_opts;
        if (uot) object["uot"] = uot;
        if (auto obj = multiplex->Build().object; !obj.isEmpty()) object["multiplex"] = obj;
        return {object, ""};
    }

    QString shadowsocks::DisplayType()
    {
        return "Shadowsocks";
    }
}
