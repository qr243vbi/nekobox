#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class WireguardBean : public AbstractBean {
    public:
        QString privateKey;
        QString publicKey;
        QString preSharedKey;
        QList<int> reserved;
        int persistentKeepalive = 0;
        QStringList localAddress;
        int MTU = 1420;
        bool useSystemInterface = false;
        int workerCount = 0;

        // Amnezia Options
        bool enable_amnezia = false;
        int junk_packet_count = 0;
        int junk_packet_min_size = 0;
        int junk_packet_max_size = 0;
        int init_packet_junk_size = 0;
        int response_packet_junk_size = 0;
        int init_packet_magic_header = 0;
        int response_packet_magic_header = 0;
        int underload_packet_magic_header = 0;
        int transport_packet_magic_header = 0;

        WireguardBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {
        }

        INIT_BEAN_MAP
            ADD_MAP("private_key", privateKey, string);
            ADD_MAP("public_key", publicKey, string);
            ADD_MAP("pre_shared_key", preSharedKey, string);
            ADD_MAP("reserved", reserved, integerList);
            ADD_MAP("persistent_keepalive", persistentKeepalive, integer);
            ADD_MAP("local_address", localAddress, stringList);
            ADD_MAP("mtu", MTU, integer);
            ADD_MAP("use_system_proxy", useSystemInterface, boolean);
            ADD_MAP("worker_count", workerCount, integer);

            ADD_MAP("enable_amnezia", enable_amnezia, boolean);
            ADD_MAP("junk_packet_count", junk_packet_count, integer);
            ADD_MAP("junk_packet_min_size", junk_packet_min_size, integer);
            ADD_MAP("junk_packet_max_size", junk_packet_max_size, integer);
            ADD_MAP("init_packet_junk_size", init_packet_junk_size, integer);
            ADD_MAP("response_packet_junk_size", response_packet_junk_size, integer);
            ADD_MAP("init_packet_magic_header", init_packet_magic_header, integer);
            ADD_MAP("response_packet_magic_header", response_packet_magic_header, integer);
            ADD_MAP("underload_packet_magic_header", underload_packet_magic_header, integer);
            ADD_MAP("transport_packet_magic_header", transport_packet_magic_header, integer);
        STOP_MAP

        QString FormatReserved() const;
/*/
        QString DisplayType() override { return "Wireguard"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        bool IsEndpoint() const override {return true;}

        virtual QString type()const override {
            return "wireguard";
        };


    private:
        bool parseWgConfig(const QString &config);
    };
} // namespace Configs
