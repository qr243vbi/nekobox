#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class WireguardBean : public AbstractBean {
    private:
        bool is_amnezia = false;
    public:
        void enableAmnezia(bool enable);
        QString privateKey;
        QString publicKey;
        QString preSharedKey;
        QList<int> reserved;
        int persistentKeepalive = 0;
        QStringList localAddress;
        int MTU = 1420;
        bool useSystemInterface = false;

        // Amnezia Options
        int junk_packet_count = 0;
        int junk_packet_min_size = 0;
        int junk_packet_max_size = 0;

        int init_packet_junk_size = 0;
        int response_packet_junk_size = 0;
        int cookie_reply_junk_size = 0;
        int transport_packet_junk_size = 0;

        QString init_packet_magic_header ;
        QString response_packet_magic_header ;
        QString cookie_reply_magic_header;
        QString transport_packet_magic_header ;

        QString i1;
        QString i2;
        QString i3;
        QString i4;
        QString i5;

        WireguardBean(Configs::ProxyEntity * entity, bool amnezia = false) : AbstractBean(entity, 0), is_amnezia(amnezia) {
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

            ADD_MAP("enable_amnezia", is_amnezia, boolean);
            ADD_MAP("jc", junk_packet_count, integer);
            ADD_MAP("jmin", junk_packet_min_size, integer);
            ADD_MAP("jmax", junk_packet_max_size, integer);

            ADD_MAP("s1", init_packet_junk_size, integer);
            ADD_MAP("s2", response_packet_junk_size, integer);
            ADD_MAP("s3", cookie_reply_junk_size, integer);
            ADD_MAP("s4", transport_packet_junk_size, integer);

            ADD_MAP("h1", init_packet_magic_header, string);
            ADD_MAP("h2", response_packet_magic_header, string);
            ADD_MAP("h3", cookie_reply_magic_header, string);
            ADD_MAP("h4", transport_packet_magic_header, string);

            ADD_MAP("i1", i1, string);
            ADD_MAP("i2", i2, string);
            ADD_MAP("i3", i3, string);
            ADD_MAP("i4", i4, string);
            ADD_MAP("i5", i5, string);
        STOP_MAP

        QString FormatReserved() const;
/*/
        QString DisplayType() override { return "Wireguard"; };
*/
        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        CoreObjOutboundBuildResult BuildCoreObjSingBoxAwg() const;

        bool TryParseJsonAwg(const QJsonObject &obj);

        bool TryParseLink(const QString &link) override;

        bool TryParseJson(const QJsonObject &obj) override;

        QString ToShareLink() const override;

        bool IsEndpoint() const override {return true;}

        virtual QString type() const override;

    private:
        bool parseWgConfig(QString config);
    };
} // namespace Configs

