#ifdef _WIN32
#include <winsock2.h>
#endif

#include <QString>
#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

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
