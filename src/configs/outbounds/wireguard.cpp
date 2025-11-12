#include "include/configs/outbounds/wireguard.h"

#include <QJsonArray>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool Peer::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("address")) address = query.queryItemValue("address");
        if (query.hasQueryItem("port")) port = query.queryItemValue("port").toInt();
        if (query.hasQueryItem("public_key")) public_key = query.queryItemValue("public_key");
        if (query.hasQueryItem("peer_public_key")) public_key = query.queryItemValue("peer_public_key");
        if (query.hasQueryItem("pre_shared_key")) pre_shared_key = query.queryItemValue("pre_shared_key");
        if (query.hasQueryItem("reserved")) {
            QString rawReserved = query.queryItemValue("reserved");
            if (!rawReserved.isEmpty()) {
                for (const auto& item : rawReserved.split("-")) {
                    int val = item.toInt();
                    if (val > 0) reserved.append(val);
                }
            }
        }
        if (query.hasQueryItem("persistent_keepalive_interval")) persistent_keepalive = query.queryItemValue("persistent_keepalive").toInt();
        
        return true;
    }

    bool Peer::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("address")) address = object["address"].toString();
        if (object.contains("port")) port = object["port"].toInt();
        if (object.contains("public_key")) public_key = object["public_key"].toString();
        if (object.contains("pre_shared_key")) pre_shared_key = object["pre_shared_key"].toString();
        if (object.contains("reserved")) {
            reserved = QJsonArray2QListInt(object["reserved"].toArray());
        }
        if (object.contains("persistent_keepalive_interval")) persistent_keepalive = object["persistent_keepalive"].toInt();
        return true;
    }

    QString Peer::ExportToLink()
    {
        QUrlQuery query;
        if (!address.isEmpty()) query.addQueryItem("address", address);
        if (port > 0) query.addQueryItem("port", QString::number(port));
        if (!public_key.isEmpty()) query.addQueryItem("public_key", public_key);
        if (!pre_shared_key.isEmpty()) query.addQueryItem("pre_shared_key", pre_shared_key);
        if (!reserved.isEmpty()) {
            QStringList reservedStr;
            for (auto val : reserved) {
                reservedStr.append(QString::number(val));
            }
            query.addQueryItem("reserved", reservedStr.join("-"));
        }
        if (persistent_keepalive > 0) query.addQueryItem("persistent_keepalive_interval", QString::number(persistent_keepalive));
        return query.toString();
    }

    QJsonObject Peer::ExportToJson()
    {
        QJsonObject object;
        if (!address.isEmpty()) object["address"] = address;
        if (port > 0) object["port"] = port;
        if (!public_key.isEmpty()) object["public_key"] = public_key;
        if (!pre_shared_key.isEmpty()) object["pre_shared_key"] = pre_shared_key;
        if (!reserved.isEmpty()) object["reserved"] = QListInt2QJsonArray(reserved);
        if (persistent_keepalive > 0) object["persistent_keepalive_interval"] = persistent_keepalive;
        return object;
    }

    BuildResult Peer::Build()
    {
        QJsonObject object;
        if (!address.isEmpty()) object["address"] = address;
        if (port > 0) object["port"] = port;
        if (!public_key.isEmpty()) object["public_key"] = public_key;
        if (!pre_shared_key.isEmpty()) object["pre_shared_key"] = pre_shared_key;
        if (!reserved.isEmpty()) object["reserved"] = QListInt2QJsonArray(reserved);
        if (persistent_keepalive > 0) object["persistent_keepalive_interval"] = persistent_keepalive;
        object["allowed_ips"] = QListStr2QJsonArray({"0.0.0.0/0", "::/0"});
        return {object, ""};
    }

    bool wireguard::ParseFromLink(const QString& link)
    {
        // Try WireGuard config file format first
        if (link.contains("[Interface]") && link.contains("[Peer]")) {
            auto lines = link.split("\n");
            for (const auto& line : lines) {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty()) continue;
                if (trimmed == "[Peer]" || trimmed == "[Interface]") {
                    continue;
                }
                if (!trimmed.contains("=")) continue;
                auto eqIdx = trimmed.indexOf("=");
                QString key = trimmed.left(eqIdx).trimmed();
                QString value = trimmed.mid(eqIdx + 1).trimmed();
                
                if (key == "PrivateKey") private_key = value;
                if (key == "Address") address = value.replace(" ", "").split(",");
                if (key == "MTU") mtu = value.toInt();
                if (key == "PublicKey") peer->public_key = value;
                if (key == "PresharedKey") peer->pre_shared_key = value;
                if (key == "PersistentKeepalive") peer->persistent_keepalive = value.toInt();
                if (key == "Endpoint") {
                    QStringList parts = value.split(":");
                    if (parts.size() >= 2) {
                        peer->address = parts[0].trimmed();
                        peer->port = parts.last().trimmed().toInt();
                        commons->server = peer->address;
                        commons->server_port = peer->port;
                    }
                }
                if (key == "S1") enable_amnezia = true, init_packet_junk_size = value.toInt();
                if (key == "S2") enable_amnezia = true, response_packet_junk_size = value.toInt();
                if (key == "Jc") enable_amnezia = true, junk_packet_count = value.toInt();
                if (key == "Jmin") enable_amnezia = true, junk_packet_min_size = value.toInt();
                if (key == "Jmax") enable_amnezia = true, junk_packet_max_size = value.toInt();
                if (key == "H1") enable_amnezia = true, init_packet_magic_header = value.toInt();
                if (key == "H2") enable_amnezia = true, response_packet_magic_header = value.toInt();
                if (key == "H3") enable_amnezia = true, underload_packet_magic_header = value.toInt();
                if (key == "H4") enable_amnezia = true, transport_packet_magic_header = value.toInt();
            }
            return !private_key.isEmpty() && !peer->public_key.isEmpty();
        }
        
        // Standard wg:// URL format
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        commons->ParseFromLink(link);

        if (query.hasQueryItem("private_key")) private_key = query.queryItemValue("private_key");
        peer->ParseFromLink(link);
        
        QString rawLocalAddr = query.queryItemValue("local_address");
        if (!rawLocalAddr.isEmpty()) {
            address = rawLocalAddr.split("-");
        }
        
        if (query.hasQueryItem("mtu")) mtu = query.queryItemValue("mtu").toInt();
        if (query.hasQueryItem("use_system_interface")) system = query.queryItemValue("use_system_interface") == "true";
        if (query.hasQueryItem("workers")) worker_count = query.queryItemValue("workers").toInt();
        if (query.hasQueryItem("udp_timeout")) udp_timeout = query.queryItemValue("udp_timeout");

        enable_amnezia = query.queryItemValue("enable_amnezia") == "true";
        if (enable_amnezia) {
            if (query.hasQueryItem("junk_packet_count")) junk_packet_count = query.queryItemValue("junk_packet_count").toInt();
            if (query.hasQueryItem("junk_packet_min_size")) junk_packet_min_size = query.queryItemValue("junk_packet_min_size").toInt();
            if (query.hasQueryItem("junk_packet_max_size")) junk_packet_max_size = query.queryItemValue("junk_packet_max_size").toInt();
            if (query.hasQueryItem("init_packet_junk_size")) init_packet_junk_size = query.queryItemValue("init_packet_junk_size").toInt();
            if (query.hasQueryItem("response_packet_junk_size")) response_packet_junk_size = query.queryItemValue("response_packet_junk_size").toInt();
            if (query.hasQueryItem("init_packet_magic_header")) init_packet_magic_header = query.queryItemValue("init_packet_magic_header").toInt();
            if (query.hasQueryItem("response_packet_magic_header")) response_packet_magic_header = query.queryItemValue("response_packet_magic_header").toInt();
            if (query.hasQueryItem("underload_packet_magic_header")) underload_packet_magic_header = query.queryItemValue("underload_packet_magic_header").toInt();
            if (query.hasQueryItem("transport_packet_magic_header")) transport_packet_magic_header = query.queryItemValue("transport_packet_magic_header").toInt();
        }

        return !(private_key.isEmpty() || peer->public_key.isEmpty() || commons->server.isEmpty());
    }

    bool wireguard::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "wireguard") return false;
        commons->ParseFromJson(object);
        if (object.contains("private_key")) private_key = object["private_key"].toString();
        if (object.contains("peer") && object["peer"].isObject()) peer->ParseFromJson(object["peer"].toObject());
        if (object.contains("address")) address = QJsonArray2QListString(object["address"].toArray());
        if (object.contains("mtu")) mtu = object["mtu"].toInt();
        if (object.contains("system")) system = object["system"].toBool();
        if (object.contains("worker_count")) worker_count = object["worker_count"].toInt();
        if (object.contains("udp_timeout")) udp_timeout = object["udp_timeout"].toString();
        return true;
    }

    QString wireguard::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("wg");
        url.setHost(peer->address);
        url.setPort(peer->port);
        if (!commons->name.isEmpty()) url.setFragment(commons->name);

        if (!private_key.isEmpty()) query.addQueryItem("private_key", private_key);
        
        if (!address.isEmpty()) query.addQueryItem("local_address", address.join("-"));
        if (mtu > 0 && mtu != 1420) query.addQueryItem("mtu", QString::number(mtu));
        if (system) query.addQueryItem("use_system_interface", "true");
        if (worker_count > 0) query.addQueryItem("workers", QString::number(worker_count));
        if (!udp_timeout.isEmpty()) query.addQueryItem("udp_timeout", udp_timeout);

        if (enable_amnezia) {
            query.addQueryItem("enable_amnezia", "true");
            if (junk_packet_count > 0) query.addQueryItem("junk_packet_count", QString::number(junk_packet_count));
            if (junk_packet_min_size > 0) query.addQueryItem("junk_packet_min_size", QString::number(junk_packet_min_size));
            if (junk_packet_max_size > 0) query.addQueryItem("junk_packet_max_size", QString::number(junk_packet_max_size));
            if (init_packet_junk_size > 0) query.addQueryItem("init_packet_junk_size", QString::number(init_packet_junk_size));
            if (response_packet_junk_size > 0) query.addQueryItem("response_packet_junk_size", QString::number(response_packet_junk_size));
            if (init_packet_magic_header > 0) query.addQueryItem("init_packet_magic_header", QString::number(init_packet_magic_header));
            if (response_packet_magic_header > 0) query.addQueryItem("response_packet_magic_header", QString::number(response_packet_magic_header));
            if (underload_packet_magic_header > 0) query.addQueryItem("underload_packet_magic_header", QString::number(underload_packet_magic_header));
            if (transport_packet_magic_header > 0) query.addQueryItem("transport_packet_magic_header", QString::number(transport_packet_magic_header));
        }
        
        mergeUrlQuery(query, commons->ExportToLink());
        mergeUrlQuery(query, peer->ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString();
    }

    QJsonObject wireguard::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "wireguard";
        if (!commons->name.isEmpty()) object["tag"] = commons->name;
        mergeJsonObjects(object, commons->dialFields->ExportToJson());
        if (!private_key.isEmpty()) object["private_key"] = private_key;
        if (!address.isEmpty()) object["address"] = QListStr2QJsonArray(address);
        if (mtu > 0) object["mtu"] = mtu;
        if (system) object["system"] = system;
        if (worker_count > 0) object["worker_count"] = worker_count;
        if (!udp_timeout.isEmpty()) object["udp_timeout"] = udp_timeout;
        
        auto peerObj = peer->ExportToJson();
        if (!peerObj.isEmpty()) {
            object["peers"] = QJsonArray({peerObj});
        }
        
        return object;
    }

    BuildResult wireguard::Build()
    {
        QJsonObject object;
        object["type"] = "wireguard";
        if (!commons->name.isEmpty()) object["tag"] = commons->name;
        mergeJsonObjects(object, commons->dialFields->Build().object);
        if (!private_key.isEmpty()) object["private_key"] = private_key;
        if (!address.isEmpty()) object["address"] = QListStr2QJsonArray(address);
        if (mtu > 0) object["mtu"] = mtu;
        if (system) object["system"] = system;
        if (worker_count > 0) object["worker_count"] = worker_count;
        if (!udp_timeout.isEmpty()) object["udp_timeout"] = udp_timeout;

        auto peerObj = peer->Build().object;
        if (!peerObj.isEmpty()) {
            object["peers"] = QJsonArray({peerObj});
        }
        return {object, ""};
    }

    QString wireguard::DisplayAddress()
    {
        return ::DisplayAddress(peer->address, peer->port > 0);
    }

    QString wireguard::DisplayType()
    {
        return "WireGuard";
    }

    bool wireguard::IsEndpoint()
    {
        return true;
    }
}
