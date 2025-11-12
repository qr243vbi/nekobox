#include "include/configs/common/Outbound.h"
#include "include/configs/common/utils.h"

namespace Configs {
    bool OutboundCommons::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;

        if (url.hasFragment()) name = url.fragment(QUrl::FullyDecoded);
        server = url.host();
        server_port = url.port();
        dialFields->ParseFromLink(link);
        return true;
    }
    bool OutboundCommons::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("tag")) name = object["tag"].toString();
        if (object.contains("server")) server = object["server"].toString();
        if (object.contains("server_port")) server_port = object["server_port"].toInt();
        dialFields->ParseFromJson(object);
        return true;
    }
    QString OutboundCommons::ExportToLink()
    {
        QUrlQuery query;
        mergeUrlQuery(query, dialFields->ExportToLink());
        return query.toString();
    }
    QJsonObject OutboundCommons::ExportToJson()
    {
        QJsonObject object;
        if (!name.isEmpty()) object["tag"] = name;
        if (!server.isEmpty()) object["server"] = server;
        if (server_port > 0) object["server_port"] = server_port;
        auto dialFieldsObj = dialFields->ExportToJson();
        mergeJsonObjects(object, dialFieldsObj);
        return object;
    }
    BuildResult OutboundCommons::Build()
    {
        return {ExportToJson(), ""};
    }
}


