#include "include/configs/common/transport.h"

#include <QJsonArray>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool Transport::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("type"))
        {
            type = query.queryItemValue("type");
            if ((type == "tcp" && query.queryItemValue("headerType") == "http") || type == "h2") type = "http";
        }
        if (query.hasQueryItem("host")) host = query.queryItemValue("host");
        if (query.hasQueryItem("path")) path = query.queryItemValue("path");
        if (query.hasQueryItem("method")) method = query.queryItemValue("method");
        if (query.hasQueryItem("headers")) headers = query.queryItemValue("headers").split(",");
        if (query.hasQueryItem("idle_timeout")) idle_timeout = query.queryItemValue("idle_timeout");
        if (query.hasQueryItem("ping_timeout")) ping_timeout = query.queryItemValue("ping_timeout");
        if (query.hasQueryItem("max_early_data")) max_early_data = query.queryItemValue("max_early_data").toInt();
        if (query.hasQueryItem("early_data_header_name")) early_data_header_name = query.queryItemValue("early_data_header_name");
        if (query.hasQueryItem("serviceName")) service_name = query.queryItemValue("serviceName");
        return true;
    }
    bool Transport::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("type")) type = object["type"].toString();
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("path")) path = object["path"].toString();
        if (object.contains("method")) method = object["method"].toString();
        if (object.contains("headers") && object["headers"].isObject()) {
            headers = jsonObjectToQStringList(object["headers"].toObject());
        }
        if (object.contains("idle_timeout")) idle_timeout = object["idle_timeout"].toString();
        if (object.contains("ping_timeout")) ping_timeout = object["ping_timeout"].toString();
        if (object.contains("max_early_data")) max_early_data = object["max_early_data"].toInt();
        if (object.contains("early_data_header_name")) early_data_header_name = object["early_data_header_name"].toString();
        if (object.contains("service_name")) service_name = object["service_name"].toString();
        return true;
    }
    QString Transport::ExportToLink()
    {
        QUrlQuery query;
        if (!type.isEmpty()) query.addQueryItem("type", type);
        if (!host.isEmpty()) query.addQueryItem("host", host);
        if (!path.isEmpty()) query.addQueryItem("path", path);
        if (!method.isEmpty()) query.addQueryItem("method", method);
        if (!headers.isEmpty()) query.addQueryItem("headers", headers.join(","));
        if (!idle_timeout.isEmpty()) query.addQueryItem("idle_timeout", idle_timeout);
        if (!ping_timeout.isEmpty()) query.addQueryItem("ping_timeout", ping_timeout);
        if (max_early_data > 0) query.addQueryItem("max_early_data", QString::number(max_early_data));
        if (!early_data_header_name.isEmpty()) query.addQueryItem("early_data_header_name", early_data_header_name);
        if (!service_name.isEmpty()) query.addQueryItem("serviceName", service_name);
        return query.toString();
    }
    QJsonObject Transport::ExportToJson()
    {
        QJsonObject object;
        if (!type.isEmpty()) object["type"] = type;
        if (!host.isEmpty()) object["host"] = host;
        if (!path.isEmpty()) object["path"] = path;
        if (!method.isEmpty()) object["method"] = method;
        if (!headers.isEmpty()) {
            object["headers"] = qStringListToJsonObject(headers);
        }
        if (!idle_timeout.isEmpty()) object["idle_timeout"] = idle_timeout;
        if (!ping_timeout.isEmpty()) object["ping_timeout"] = ping_timeout;
        if (max_early_data > 0) object["max_early_data"] = max_early_data;
        if (!early_data_header_name.isEmpty()) object["early_data_header_name"] = early_data_header_name;
        if (!service_name.isEmpty()) object["service_name"] = service_name;
        return object;
    }
    BuildResult Transport::Build()
    {
        return {ExportToJson(), ""};
    }
}


