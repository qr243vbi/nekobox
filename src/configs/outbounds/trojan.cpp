#include "include/configs/outbounds/trojan.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool Trojan::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;

        commons->ParseFromLink(link);
        password = url.userName();
        tls->ParseFromLink(link);
        transport->ParseFromLink(link);
        multiplex->ParseFromLink(link);
        return true;
    }
    bool Trojan::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "trojan") return false;
        commons->ParseFromJson(object);
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (object.contains("transport")) transport->ParseFromJson(object["transport"].toObject());
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return true;
    }
    QString Trojan::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setHost(commons->server);
        url.setPort(commons->server_port);
        url.setScheme("trojan");
        if (!commons->name.isEmpty()) url.setFragment(commons->name);
        url.setUserName(password);
        if (tls->enabled) mergeUrlQuery(query, tls->ExportToLink());
        if (!transport->type.isEmpty()) mergeUrlQuery(query, transport->ExportToLink());
        if (multiplex->enabled) mergeUrlQuery(query, multiplex->ExportToLink());
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString();
    }
    QJsonObject Trojan::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "trojan";
        mergeJsonObjects(object, commons->ExportToJson());
        if (!password.isEmpty()) object["password"] = password;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        if (!transport->type.isEmpty()) object["transport"] = transport->ExportToJson();
        if (multiplex->enabled) object["multiplex"] = multiplex->ExportToJson();
        return object;
    }
    BuildResult Trojan::Build()
    {
        QJsonObject object;
        object["type"] = "trojan";
        mergeJsonObjects(object, commons->Build().object);
        if (!password.isEmpty()) object["password"] = password;
        if (tls->enabled) object["tls"] = tls->Build().object;
        if (!transport->type.isEmpty()) object["transport"] = transport->Build().object;
        if (auto obj = multiplex->Build().object; !obj.isEmpty()) object["multiplex"] = obj;
        return {object, ""};
    }

    QString Trojan::DisplayType()
    {
        return "Trojan";
    }
}


