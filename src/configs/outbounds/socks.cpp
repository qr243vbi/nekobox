#include "include/configs/outbounds/socks.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool socks::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        commons->ParseFromLink(link);
        if (query.hasQueryItem("version"))
        {
            version = query.queryItemValue("version").toInt();
        } else
        {
            if (url.scheme() == "socks4") version = 4;
            if (url.scheme() == "socks5") version = 5;
        }
        
        // Handle v2rayN format (base64 encoded username)
        if (!url.password().isEmpty() || !url.userName().isEmpty()) {
            username = url.userName();
            password = url.password();
            
            // Check if username is base64 encoded (v2rayN format)
            if (password.isEmpty() && !username.isEmpty()) {
                QString decoded = DecodeB64IfValid(username);
                if (!decoded.isEmpty()) {
                    username = SubStrBefore(decoded, ":");
                    password = SubStrAfter(decoded, ":");
                }
            }
        }

        if (query.hasQueryItem("uot")) uot = query.queryItemValue("uot") == "true" || query.queryItemValue("uot").toInt() > 0;
        
        // Default port
        if (commons->server_port == 0) commons->server_port = 1080;

        return !commons->server.isEmpty();
    }

    bool socks::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "socks") return false;
        commons->ParseFromJson(object);
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("version")) version = object["version"].toInt();
        if (object.contains("uot")) uot = object["uot"].toBool();
        return true;
    }

    QString socks::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        
        // Determine scheme based on SOCKS version (default to socks5)
        QString scheme = "socks5";
        if (version == 4) {
            scheme = "socks4";
        }
        url.setScheme(scheme);
        
        url.setHost(commons->server);
        url.setPort(commons->server_port);
        if (!commons->name.isEmpty()) url.setFragment(commons->name);
        
        if (!username.isEmpty()) url.setUserName(username);
        if (!password.isEmpty()) url.setPassword(password);
        if (uot) query.addQueryItem("uot", "1");
        
        mergeUrlQuery(query, commons->ExportToLink());
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString();
    }

    QJsonObject socks::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "socks";
        mergeJsonObjects(object, commons->ExportToJson());
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (version == 4) object["version"] = "4";
        if (uot) object["uot"] = uot;
        return object;
    }

    BuildResult socks::Build()
    {
        QJsonObject object;
        object["type"] = "socks";
        mergeJsonObjects(object, commons->Build().object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (version == 4) object["version"] = "4";
        if (uot) object["uot"] = uot;
        return {object, ""};
    }

    QString socks::DisplayType()
    {
        return "Socks";
    }
}
