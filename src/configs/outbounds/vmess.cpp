#include "include/configs/outbounds/vmess.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool vmess::ParseFromLink(const QString& link)
    {
        // Try V2RayN format first (base64 encoded JSON)
        QString linkN = DecodeB64IfValid(SubStrAfter(link, "vmess://"));
        if (!linkN.isEmpty()) {
            auto objN = QString2QJsonObject(linkN);
            if (!objN.isEmpty()) {
                uuid = objN["id"].toString();
                commons->server = objN["add"].toString();
                commons->server_port = objN["port"].toVariant().toInt();
                commons->name = objN["ps"].toString();
                alter_id = objN["aid"].toVariant().toInt();
                
                QString net = objN["net"].toString();
                if (net == "h2") net = "http";
                transport->type = net;
                transport->host = objN["host"].toString();
                transport->path = objN["path"].toString();
                
                QString scy = objN["scy"].toString();
                if (!scy.isEmpty()) security = scy;
                
                QString tlsStr = objN["tls"].toString();
                if (tlsStr == "tls") {
                    tls->enabled = true;
                    tls->server_name = objN["sni"].toString();
                }
                
                return !(uuid.isEmpty() || commons->server.isEmpty());
            }
        }
        
        // Standard VMess URL format
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        commons->ParseFromLink(link);
        uuid = url.userName();
        if (commons->server_port == 0) commons->server_port = 443;

        security = GetQueryValue(query, "encryption", "auto");

        transport->ParseFromLink(link);
        
        tls->ParseFromLink(link);
        if (!tls->server_name.isEmpty()) {
            tls->enabled = true;
        }
        
        multiplex->ParseFromLink(link);
        
        if (query.hasQueryItem("alterId")) alter_id = query.queryItemValue("alterId").toInt();
        if (query.hasQueryItem("globalPadding")) global_padding = query.queryItemValue("globalPadding") == "true";
        if (query.hasQueryItem("authenticatedLength")) authenticated_length = query.queryItemValue("authenticatedLength") == "true";
        if (query.hasQueryItem("packetEncoding")) packet_encoding = query.queryItemValue("packetEncoding");

        return !(uuid.isEmpty() || commons->server.isEmpty());
    }

    bool vmess::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "vmess") return false;
        commons->ParseFromJson(object);
        if (object.contains("uuid")) uuid = object["uuid"].toString();
        if (object.contains("security")) security = object["security"].toString();
        if (object.contains("alter_id")) alter_id = object["alter_id"].toInt();
        if (object.contains("alter-id")) alter_id = object["alter-id"].toInt();
        if (object.contains("global_padding")) global_padding = object["global_padding"].toBool();
        if (object.contains("global-padding")) global_padding = object["global-padding"].toBool();
        if (object.contains("authenticated_length")) authenticated_length = object["authenticated_length"].toBool();
        if (object.contains("packet_encoding")) packet_encoding = object["packet_encoding"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (object.contains("transport")) transport->ParseFromJson(object["transport"].toObject());
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return true;
    }

    QString vmess::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("vmess");
        url.setUserName(uuid);
        url.setHost(commons->server);
        url.setPort(commons->server_port);
        if (!commons->name.isEmpty()) url.setFragment(commons->name);

        if (security != "auto") query.addQueryItem("encryption", security);

        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, transport->ExportToLink());
        mergeUrlQuery(query, multiplex->ExportToLink());
        
        if (alter_id > 0) query.addQueryItem("alterId", QString::number(alter_id));
        if (global_padding) query.addQueryItem("globalPadding", "true");
        if (authenticated_length) query.addQueryItem("authenticatedLength", "true");
        if (!packet_encoding.isEmpty()) query.addQueryItem("packetEncoding", packet_encoding);
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString();
    }

    QJsonObject vmess::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "vmess";
        mergeJsonObjects(object, commons->ExportToJson());
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (security != "auto") object["security"] = security;
        if (alter_id > 0) object["alter_id"] = alter_id;
        if (global_padding) object["global_padding"] = global_padding;
        if (authenticated_length) object["authenticated_length"] = authenticated_length;
        if (!packet_encoding.isEmpty()) object["packet_encoding"] = packet_encoding;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        if (!transport->type.isEmpty()) object["transport"] = transport->ExportToJson();
        if (multiplex->enabled) object["multiplex"] = multiplex->ExportToJson();
        return object;
    }

    BuildResult vmess::Build()
    {
        QJsonObject object;
        object["type"] = "vmess";
        mergeJsonObjects(object, commons->Build().object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (security != "auto") object["security"] = security;
        if (alter_id > 0) object["alter_id"] = alter_id;
        if (global_padding) object["global_padding"] = global_padding;
        if (authenticated_length) object["authenticated_length"] = authenticated_length;
        if (!packet_encoding.isEmpty()) object["packet_encoding"] = packet_encoding;
        if (tls->enabled) object["tls"] = tls->Build().object;
        if (!transport->type.isEmpty()) object["transport"] = transport->Build().object;
        if (auto obj = multiplex->Build().object; !obj.isEmpty()) object["multiplex"] = obj;
        return {object, ""};
    }

    QString vmess::DisplayType()
    {
        return "VMess";
    }
}
