#ifdef _WIN32
#include <winsock2.h>
#endif

#include <QString>
#include <nekobox/dataStore/ProxyEntity.hpp>

#include <nekobox/configs/proxy/WireguardBean.h>

namespace Configs {
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
