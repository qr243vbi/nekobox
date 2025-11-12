#include "include/configs/outbounds/http.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool http::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        commons->ParseFromLink(link);
        username = url.userName();
        password = url.password();
        if (query.hasQueryItem("path")) path = query.queryItemValue("path");
        if (query.hasQueryItem("headers")) headers = query.queryItemValue("headers").split(",");
        if (url.scheme() == "https" || query.queryItemValue("security") == "tls")
        {
            tls->ParseFromLink(link);
            tls->enabled = true; // force enable in case scheme is https and no security queryValue is set
        }
        if (commons->server_port == 0) commons->server_port = 443;
        return true;
    }
    bool http::ParseFromJson(const QJsonObject& object)
    {
        if (object.empty() || object["type"] != "http") return false;
        commons->ParseFromJson(object);
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("path")) path = object["path"].toString();
        if (object.contains("headers") && object["headers"].isObject()) headers = jsonObjectToQStringList(object["headers"].toObject());
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }
    QString http::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme(tls->enabled ? "https" : "http");
        url.setHost(commons->server);
        url.setPort(commons->server_port);
        if (!commons->name.isEmpty()) url.setFragment(commons->name);
        if (!username.isEmpty()) url.setUserName(username);
        if (!password.isEmpty()) url.setPassword(password);
        if (!path.isEmpty()) query.addQueryItem("path", path);
        if (!headers.empty()) query.addQueryItem("headers", headers.join(","));
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, commons->ExportToLink());
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString();
    }
    QJsonObject http::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "http";
        mergeJsonObjects(object, commons->ExportToJson());
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!path.isEmpty()) object["path"] = path;
        if (auto headerObj = qStringListToJsonObject(headers); !headerObj.isEmpty()) object["headers"] = headerObj;
        if (auto tlsObj = tls->ExportToJson(); !tlsObj.isEmpty()) object["tls"] = tlsObj;
        return object;
    }
    BuildResult http::Build()
    {
        QJsonObject object;
        object["type"] = "http";
        mergeJsonObjects(object, commons->Build().object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!path.isEmpty()) object["path"] = path;
        if (auto headerObj = qStringListToJsonObject(headers); !headerObj.isEmpty()) object["headers"] = headerObj;
        if (auto tlsObj = tls->Build().object; !tlsObj.isEmpty()) object["tls"] = tlsObj;
        return {object, ""};
    }

    QString http::DisplayType()
    {
        return "HTTP";
    }
}


