



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


#pragma once

#include "AbstractBean.hpp"

namespace Configs {

    class AmneziaWGBean : public AbstractBean {
    public:
        QString privateKey;
        QStringList address;
        int MTU = 1420;
        int listenPort = 0;
        bool useIntegratedTun = false;

        // AmneziaWG parameters
        int jc = 0;
        int jmin = 0;
        int jmax = 0;

        int s1 = 0;
        int s2 = 0;
        int s3 = 0;
        int s4 = 0;

        QString h1;
        QString h2;
        QString h3;
        QString h4;

        QString i1;
        QString i2;
        QString i3;
        QString i4;
        QString i5;

        class Peer: public JsonStore {
        public:
            QString address;
            int port = 0;
            QString publicKey;
            QString presharedKey;
            QStringList allowedIPs;
            int persistentKeepaliveInterval = 0;

            DECLARE_STORE_TYPE(NoSave)
            NEW_MAP
                ADD_MAP("address", address, string);
                ADD_MAP("public_key", publicKey, string);
                ADD_MAP("preshared_key", presharedKey, string);
                ADD_MAP("port", port, integer);
                ADD_MAP("persistent_keepalive_interval", persistentKeepaliveInterval, integer);
                ADD_MAP("allowed_ips", address, stringList);
            STOP_MAP
        };

        QJsonStoreList<Peer> peers;

        AmneziaWGBean(Configs::ProxyEntity *entity)
            : AbstractBean(entity, 0) {
        }

        INIT_BEAN_MAP
            ADD_MAP("private_key", privateKey, string);
            ADD_MAP("addresses", address, stringList);
            ADD_MAP("mtu", MTU, integer);
            ADD_MAP("listen_port", listenPort, integer);
            ADD_MAP("useIntegratedTun", useIntegratedTun, boolean);

            ADD_MAP("jc", jc, integer);
            ADD_MAP("jmin", jmin, integer);
            ADD_MAP("jmax", jmax, integer);

            ADD_MAP("s1", s1, integer);
            ADD_MAP("s2", s2, integer);
            ADD_MAP("s3", s3, integer);
            ADD_MAP("s4", s4, integer);

            ADD_MAP("h1", h1, string);
            ADD_MAP("h2", h2, string);
            ADD_MAP("h3", h3, string);
            ADD_MAP("h4", h4, string);

            ADD_MAP("i1", i1, string);
            ADD_MAP("i2", i2, string);
            ADD_MAP("i3", i3, string);
            ADD_MAP("i4", i4, string);
            ADD_MAP("i5", i5, string);

            ADD_MAP("peers", peers, objectList);
        STOP_MAP

        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        bool IsEndpoint() const override {
            return true;
        }

        virtual QString type() const override {
            return "amneziawg";
        }
    };

} // namespace Configs