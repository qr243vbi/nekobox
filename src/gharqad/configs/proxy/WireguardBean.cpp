



#include <QString>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/WireguardBean.h>

namespace Configs {



    CoreObjOutboundBuildResult WireguardBean::BuildCoreObjSingBox() const {
        CoreObjOutboundBuildResult result;
        using namespace To_CoreObj_box;
        auto tun_name = "nekobox-wg";

        QJsonObject peer{
            {"address", entity->serverAddress},
            {"port", entity->serverPort},
            {"public_key", publicKey},
            {"pre_shared_key", preSharedKey},
            {"reserved", QListInt2QJsonArray(reserved)},
            {"allowed_ips", QListStr2QJsonArray({"0.0.0.0/0", "::/0"})},
            {"persistent_keepalive_interval", persistentKeepalive},
        };
        QJsonObject outbound{
            {"type", "wireguard"},
            {"name", tun_name},
            {"address", QListStr2QJsonArray(localAddress)},
            {"private_key", privateKey},
            {"peers", QJsonArray{peer}},
            {"mtu", MTU},
            {"system", useSystemInterface},
            {"workers", workerCount}
        };
        if (enable_amnezia)
        {
            outbound["junk_packet_count"] = junk_packet_count;
            outbound["junk_packet_min_size"] = junk_packet_min_size;
            outbound["junk_packet_max_size"] = junk_packet_max_size;
            outbound["init_packet_junk_size"] = init_packet_junk_size;
            outbound["response_packet_junk_size"] = response_packet_junk_size;
            outbound["init_packet_magic_header"] = init_packet_magic_header;
            outbound["response_packet_magic_header"] = response_packet_magic_header;
            outbound["underload_packet_magic_header"] = underload_packet_magic_header;
            outbound["transport_packet_magic_header"] = transport_packet_magic_header;
        }

        result.outbound = outbound;
        return result;
    }

        bool WireguardBean::TryParseLink(const QString &link) {
        using namespace From_Link;
        if (parseWgConfig(link)) return true;

        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);
        add_default_fields(url, entity);

        privateKey = query.queryItemValue("private_key");
        publicKey = query.queryItemValue("peer_public_key");
        preSharedKey = query.queryItemValue("pre_shared_key");
        auto rawReserved = query.queryItemValue("reserved");
        if (!rawReserved.isEmpty()) {
            for (const auto &item: rawReserved.split("-")) reserved += item.toInt();
        }
        auto rawLocalAddr = query.queryItemValue("local_address");
        if (!rawLocalAddr.isEmpty()) {
            for (const auto &item: rawLocalAddr.split("-")) localAddress += item;
        }
        persistentKeepalive = GetQueryIntValue(query, "persistent_keepalive", GetQueryIntValue(query, "persistent_keepalive_interval"));
        MTU = GetQueryIntValue(query, "mtu");
        set_boolean("use_system_interface", useSystemInterface, query);
        workerCount = GetQueryIntValue(query, "workers");

        set_boolean("enable_amnezia", enable_amnezia, query);
        junk_packet_count = GetQueryIntValue(query, "junk_packet_count");
        junk_packet_min_size = GetQueryIntValue(query, "junk_packet_min_size");
        junk_packet_max_size = GetQueryIntValue(query, "junk_packet_max_size");
        init_packet_junk_size = GetQueryIntValue(query, "init_packet_junk_size");
        response_packet_junk_size = GetQueryIntValue(query, "response_packet_junk_size");
        init_packet_magic_header = GetQueryIntValue(query, "init_packet_magic_header");
        response_packet_magic_header = GetQueryIntValue(query, "response_packet_magic_header");
        underload_packet_magic_header = GetQueryIntValue(query, "underload_packet_magic_header");
        transport_packet_magic_header = GetQueryIntValue(query, "transport_packet_magic_header");

        return true;
    }

    bool WireguardBean::TryParseJson(const QJsonObject& obj)
    {
            using namespace Configs::From_Json;
        add_default_fields(this->entity, obj);
        auto peers = obj["peers"].toArray();
        if (peers.empty()) return false;
        publicKey = peers[0].toObject()["public_key"].toString();
        reserved = QJsonArray2QListInt(peers[0].toObject()["reserved"].toArray());
        persistentKeepalive = peers[0].toObject()["persistent_keepalive_interval"].toInt();
        workerCount = obj["workers"].toInt();
        privateKey = obj["private_key"].toString();
        localAddress = QJsonArray2QListStr(obj["address"].toArray());
        MTU = obj["mtu"].toInt();
        useSystemInterface = obj["system"].toBool();

        junk_packet_count = obj["junk_packet_count"].toInt();
        junk_packet_min_size = obj["junk_packet_min_size"].toInt();
        junk_packet_max_size = obj["junk_packet_max_size"].toInt();
        init_packet_junk_size = obj["init_packet_junk_size"].toInt();
        response_packet_junk_size = obj["response_packet_junk_size"].toInt();
        init_packet_magic_header = obj["init_packet_magic_header"].toInt();
        response_packet_magic_header = obj["response_packet_magic_header"].toInt();
        underload_packet_magic_header = obj["underload_packet_magic_header"].toInt();
        transport_packet_magic_header = obj["transport_packet_magic_header"].toInt();
        if (junk_packet_count > 0 || junk_packet_min_size > 0 || junk_packet_max_size > 0)
        {
            enable_amnezia = true;
        }

        return true;
    }


    QString WireguardBean::ToShareLink() const {
            using namespace Configs::To_Link;

        QUrl url;
        url.setScheme("wg");
        add_default_fields(url, this);
        QUrlQuery query;
        add_query_nonempty("private_key", query, privateKey);
        add_query_nonempty("peer_public_key", query, publicKey);
        add_query_nonempty("pre_shared_key", query, preSharedKey);
        add_query_nonempty("reserved", query, FormatReserved());
        add_query_int("persistent_keepalive", query, (persistentKeepalive));
        add_query_int("mtu", query, (MTU));
        add_query_boolean("use_system_interface", query, useSystemInterface);
        add_query_nonempty("local_address", query, localAddress.join("-"));
        add_query_int("workers", query, (workerCount));
        if (enable_amnezia)
        {
            query.addQueryItem("enable_amnezia", "true");
            add_query_int("junk_packet_count", query, (junk_packet_count));
            add_query_int("junk_packet_min_size", query, (junk_packet_min_size));
            add_query_int("junk_packet_max_size", query, (junk_packet_max_size));
            add_query_int("init_packet_junk_size", query, (init_packet_junk_size));
            add_query_int("response_packet_junk_size", query, (response_packet_junk_size));
            add_query_int("init_packet_magic_header", query, (init_packet_magic_header));
            add_query_int("response_packet_magic_header", query, (response_packet_magic_header));
            add_query_int("underload_packet_magic_header", query, (underload_packet_magic_header));
            add_query_int("transport_packet_magic_header", query, (transport_packet_magic_header));
        }
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }
    
        QString WireguardBean::FormatReserved() const {
            QString res = "";
            for (int i=0;i<reserved.size();i++) {
                res += QString::number(reserved[i]);
                if (i != reserved.size() - 1) {
                    res += "-";
                }
            }
            return res;
        }

        bool WireguardBean::parseWgConfig(const QString & config)
        {
            if (!config.contains("[Interface]") || !config.contains("[Peer]")) return false;
            auto lines = config.split("\n");
            for (const auto& line : lines)
            {
                if (line.trimmed().isEmpty()) continue;
                if (line.contains("[Interface]") || line.contains("[Peer]")) continue;
                if (!line.contains("=")) return false;
                auto eqIdx = line.indexOf("=");
                if (line.startsWith("PrivateKey"))
                {
                    privateKey = line.mid(eqIdx + 1).trimmed();
                }
                if (line.startsWith("Address"))
                {
                    auto addresses = line.mid(eqIdx + 1).trimmed().split(",");
                    for (const auto& address : addresses) localAddress.append(address.trimmed());
                }
                if (line.startsWith("MTU"))
                {
                    MTU = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("PublicKey"))
                {
                    publicKey = line.mid(eqIdx + 1).trimmed();
                }
                if (line.startsWith("PresharedKey"))
                {
                    preSharedKey = line.mid(eqIdx + 1).trimmed();
                }
                if (line.startsWith("PersistentKeepalive"))
                {
                    persistentKeepalive = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("Endpoint"))
                {
                    auto addrPort = line.mid(eqIdx + 1).trimmed();
                    if (!addrPort.contains(":")) return false;
                    entity->serverAddress = addrPort.split(":")[0].trimmed();
                    entity->serverPort = addrPort.split(":")[1].trimmed().toInt();
                }
                if (line.startsWith("S1"))
                {
                    enable_amnezia = true;
                    init_packet_junk_size = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("S2"))
                {
                    enable_amnezia = true;
                    response_packet_junk_size = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("Jc"))
                {
                    enable_amnezia = true;
                    junk_packet_count = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("Jmin"))
                {
                    enable_amnezia = true;
                    junk_packet_min_size = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("Jmax"))
                {
                    enable_amnezia = true;
                    junk_packet_max_size = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("H1"))
                {
                    enable_amnezia = true;
                    init_packet_magic_header = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("H2"))
                {
                    enable_amnezia = true;
                    response_packet_magic_header = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("H3"))
                {
                    enable_amnezia = true;
                    underload_packet_magic_header = line.mid(eqIdx + 1).toInt();
                }
                if (line.startsWith("H4"))
                {
                    enable_amnezia = true;
                    transport_packet_magic_header = line.mid(eqIdx + 1).toInt();
                }
            }
            entity->name = "Wg file config";
            return true;
        };
}
