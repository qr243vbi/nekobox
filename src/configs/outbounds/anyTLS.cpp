#include "include/configs/outbounds/anyTLS.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool anyTLS::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        commons->ParseFromLink(link);
        password = url.userName();
        
        if (commons->server_port == 0) commons->server_port = 443;
        
        if (query.hasQueryItem("idle_session_check_interval")) idle_session_check_interval = query.queryItemValue("idle_session_check_interval");
        if (query.hasQueryItem("idle_session_timeout")) idle_session_timeout = query.queryItemValue("idle_session_timeout");
        if (query.hasQueryItem("min_idle_session")) min_idle_session = query.queryItemValue("min_idle_session").toInt();
        
        tls->ParseFromLink(link);
        tls->enabled = true; // anyTLS always uses tls

        return true;
    }

    bool anyTLS::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "anytls") return false;
        commons->ParseFromJson(object);
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("idle_session_check_interval")) idle_session_check_interval = object["idle_session_check_interval"].toString();
        if (object.contains("idle_session_timeout")) idle_session_timeout = object["idle_session_timeout"].toString();
        if (object.contains("min_idle_session")) min_idle_session = object["min_idle_session"].toInt();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    QString anyTLS::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("anytls");
        url.setUserName(password);
        url.setHost(commons->server);
        url.setPort(commons->server_port);
        if (!commons->name.isEmpty()) url.setFragment(commons->name);

        if (!idle_session_check_interval.isEmpty()) query.addQueryItem("idle_session_check_interval", idle_session_check_interval);
        if (!idle_session_timeout.isEmpty()) query.addQueryItem("idle_session_timeout", idle_session_timeout);
        if (min_idle_session > 0) query.addQueryItem("min_idle_session", QString::number(min_idle_session));
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, commons->ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString();
    }

    QJsonObject anyTLS::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "anytls";
        mergeJsonObjects(object, commons->ExportToJson());
        if (!password.isEmpty()) object["password"] = password;
        if (!idle_session_check_interval.isEmpty()) object["idle_session_check_interval"] = idle_session_check_interval;
        if (!idle_session_timeout.isEmpty()) object["idle_session_timeout"] = idle_session_timeout;
        if (min_idle_session > 0) object["min_idle_session"] = min_idle_session;
        object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult anyTLS::Build()
    {
        QJsonObject object;
        object["type"] = "anytls";
        mergeJsonObjects(object, commons->Build().object);
        if (!password.isEmpty()) object["password"] = password;
        if (!idle_session_check_interval.isEmpty()) object["idle_session_check_interval"] = idle_session_check_interval;
        if (!idle_session_timeout.isEmpty()) object["idle_session_timeout"] = idle_session_timeout;
        if (min_idle_session > 0) object["min_idle_session"] = min_idle_session;
        object["tls"] = tls->Build().object;
        return {object, ""};
    }

    QString anyTLS::DisplayType()
    {
        return "AnyTLS";
    }
}
