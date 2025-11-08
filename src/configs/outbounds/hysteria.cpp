#include "include/configs/outbounds/hysteria.h"

#include <QJsonArray>
#include <QUrlQuery>
#include <3rdparty/URLParser/url_parser.h>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool hysteria::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid())
        {
            if(!url.errorString().startsWith("Invalid port"))
                return false;
            commons->server_port = 0;
            server_ports = QString::fromStdString(URLParser::Parse((link.split("?")[0] + "/").toStdString()).port).split(",");
            for (auto & serverPort : server_ports) {
                serverPort.replace("-", ":");
            }
        }
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        commons->ParseFromLink(link);
        
        if (query.hasQueryItem("upmbps")) up_mbps = query.queryItemValue("upmbps").toInt();
        if (query.hasQueryItem("downmbps")) down_mbps = query.queryItemValue("downmbps").toInt();
        if (query.hasQueryItem("obfsParam")) obfs = QUrl::fromPercentEncoding(query.queryItemValue("obfsParam").toUtf8());
        if (query.hasQueryItem("auth")) auth_str = query.queryItemValue("auth");
        if (query.hasQueryItem("recv_window_conn")) recv_window_conn = query.queryItemValue("recv_window_conn").toInt();
        if (query.hasQueryItem("recv_window")) recv_window = query.queryItemValue("recv_window").toInt();
        if (query.hasQueryItem("disable_mtu_discovery")) disable_mtu_discovery = query.queryItemValue("disable_mtu_discovery") == "true";
        if (query.hasQueryItem("hop_interval")) hop_interval = query.queryItemValue("hop_interval");
        if (query.hasQueryItem("mport")) {
            server_ports = query.queryItemValue("mport").split(",");
            for (auto & server_port : server_ports) {
                server_port.replace("-", ":");
            }
        }
        
        tls->ParseFromLink(link);
        
        if (commons->server_port == 0 && server_ports.isEmpty()) commons->server_port = 443;

        return true;
    }

    bool hysteria::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "hysteria") return false;
        commons->ParseFromJson(object);
        if (object.contains("server_ports")) {
            server_ports = QJsonArray2QListString(object["server_ports"].toArray());
        }
        if (object.contains("hop_interval")) hop_interval = object["hop_interval"].toString();
        if (object.contains("up_mbps")) up_mbps = object["up_mbps"].toInt();
        if (object.contains("down_mbps")) down_mbps = object["down_mbps"].toInt();
        if (object.contains("obfs")) obfs = object["obfs"].toString();
        if (object.contains("auth")) auth = object["auth"].toString();
        if (object.contains("auth_str")) auth_str = object["auth_str"].toString();
        if (object.contains("recv_window_conn")) recv_window_conn = object["recv_window_conn"].toInt();
        if (object.contains("recv_window")) recv_window = object["recv_window"].toInt();
        if (object.contains("disable_mtu_discovery")) disable_mtu_discovery = object["disable_mtu_discovery"].toBool();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    QString hysteria::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("hysteria");
        url.setHost(commons->server);
        url.setPort(0);
        
        if (!server_ports.isEmpty()) {
            QStringList portList;
            for (const auto& port : server_ports) {
                QString modified = port;
                modified.replace(":", "-");
                portList.append(modified);
            }
            url.setPort(0);
            query.addQueryItem("mport", portList.join(","));
        } else {
            url.setPort(commons->server_port);
        }
        
        if (!commons->name.isEmpty()) url.setFragment(commons->name);
        
        if (up_mbps > 0) query.addQueryItem("upmbps", QString::number(up_mbps));
        if (down_mbps > 0) query.addQueryItem("downmbps", QString::number(down_mbps));
        if (!obfs.isEmpty()) {
            query.addQueryItem("obfsParam", QUrl::toPercentEncoding(obfs));
        }
        if (!auth_str.isEmpty()) query.addQueryItem("auth", auth_str);
        if (recv_window_conn > 0) query.addQueryItem("recv_window_conn", QString::number(recv_window_conn));
        if (recv_window > 0) query.addQueryItem("recv_window", QString::number(recv_window));
        if (disable_mtu_discovery) query.addQueryItem("disable_mtu_discovery", "true");
        if (!hop_interval.isEmpty()) query.addQueryItem("hop_interval", hop_interval);
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, commons->ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        
        QString result = url.toString();
        if (!server_ports.isEmpty()) {
            result = result.replace(":0?", ":" + query.queryItemValue("mport") + "?");
        }
        return result;
    }

    QJsonObject hysteria::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "hysteria";
        mergeJsonObjects(object, commons->ExportToJson());
        if (!server_ports.isEmpty()) object["server_ports"] = QListStr2QJsonArray(server_ports);
        if (!hop_interval.isEmpty()) object["hop_interval"] = hop_interval;
        if (up_mbps > 0) object["up_mbps"] = up_mbps;
        if (down_mbps > 0) object["down_mbps"] = down_mbps;
        if (!obfs.isEmpty()) object["obfs"] = obfs;
        if (!auth.isEmpty()) object["auth"] = auth;
        if (!auth_str.isEmpty()) object["auth_str"] = auth_str;
        if (recv_window_conn > 0) object["recv_window_conn"] = recv_window_conn;
        if (recv_window > 0) object["recv_window"] = recv_window;
        if (disable_mtu_discovery) object["disable_mtu_discovery"] = disable_mtu_discovery;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult hysteria::Build()
    {
        QJsonObject object;
        object["type"] = "hysteria";
        mergeJsonObjects(object, commons->Build().object);
        if (!server_ports.isEmpty()) object["server_ports"] = QListStr2QJsonArray(server_ports);
        if (!hop_interval.isEmpty()) object["hop_interval"] = hop_interval;
        if (up_mbps > 0) object["up_mbps"] = up_mbps;
        if (down_mbps > 0) object["down_mbps"] = down_mbps;
        if (!obfs.isEmpty()) object["obfs"] = obfs;
        if (!auth.isEmpty()) object["auth"] = auth;
        if (!auth_str.isEmpty()) object["auth_str"] = auth_str;
        if (recv_window_conn > 0) object["recv_window_conn"] = recv_window_conn;
        if (recv_window > 0) object["recv_window"] = recv_window;
        if (disable_mtu_discovery) object["disable_mtu_discovery"] = disable_mtu_discovery;
        if (tls->enabled) object["tls"] = tls->Build().object;
        return {object, ""};
    }

    QString hysteria::DisplayType()
    {
        return "Hysteria";
    }
}
