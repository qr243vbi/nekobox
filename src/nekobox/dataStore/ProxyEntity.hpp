#pragma once

#include "Utils.hpp"
#include "nekobox/dataStore/Configs.hpp"
#include "nekobox/global/CountryHelper.hpp"
#include "nekobox/stats/traffic/TrafficData.hpp"
#include "nekobox/configs/proxy/AbstractBean.hpp"
#include <QColor>
#include <memory>
#include "nekobox/configs/proxy/ExtraCore.h"

namespace Configs {
    class SocksHttpBean;

    class ShadowSocksBean;

    class VMessBean;

    class TrojanVLESSBean;

    class NaiveBean;

    class QUICBean;

    class AnyTLSBean;

    class MieruBean;

    class ShadowTLSBean;

    class WireguardBean;

    class TailscaleBean;

    class SSHBean;

    class TorBean;

    class CustomBean;

    class ChainBean;
}; // namespace Configs

namespace Configs {
    class ProxyEntity : public JsonStore {
    private:
        std::weak_ptr<Configs::AbstractBean> weak_bean;
        std::shared_ptr<Configs::AbstractBean> strong_bean;
    public:
        virtual ConfJsMap _map() override;
        virtual bool Save() override;

        QString type;

        QString name = "";
        QString serverAddress = "127.0.0.1";
        int serverPort = 1080;

        int id = -1;
        int gid = 0;
        int latencyInt = 0;
        int latencyOrder = 0;

        bool is_working = false;
        bool invalid = false;
        QString bean_cfg;
        QString dl_speed;
        QString ul_speed;
        QString test_country;
        std::shared_ptr<const Configs::AbstractBean> bean() const;
        virtual void UnknownKeyHash(const QByteArray & array) override;
        std::shared_ptr<Stats::TrafficData> traffic_data = std::make_shared<Stats::TrafficData>("");
        QString full_test_report;

        ProxyEntity(const QString &type_);
        virtual ~ProxyEntity() override;

        qint64 last_auto_test_time = 0;

        template<typename A>
        std::shared_ptr<A> unlock(std::shared_ptr<const A> bean){
            auto ret = this->weak_bean.lock();
            if ((void*)ret.get() == (void*)bean.get()){
                if (ret != nullptr){
                    ret->save_control_no_save = false;
                }
                return std::static_pointer_cast<A>(ret);
            }
            return nullptr;
        }

        [[nodiscard]] virtual QString DisplayAddress(){
            return serverAddress + ":" + QString::number(serverPort);
        }

        [[nodiscard]] virtual QString DisplayName(){
            return name;
        }

        [[nodiscard]] virtual QString DisplayCoreType(){
            return software_core_name;
        }

        [[nodiscard]] virtual QString DisplayType(){
            return this->type;
        }

        [[nodiscard]] virtual QString DisplayTypeAndName(){
            return this->type + " " + name;
        }

        [[nodiscard]] QString DisplayTestResult() const;

   //     [[nodiscard]] QColor DisplayLatencyColor() const;

        #define SocksHTTPBean SocksHttpBean
        #define cast_func(X)         \
        [[nodiscard]] std::shared_ptr<const Configs::X##Bean> X##Bean() const;

        cast_func(Chain)
        cast_func(SocksHttp)
        cast_func(ShadowSocks)
        cast_func(VMess)
        cast_func(TrojanVLESS)
 //       cast_func(Naive)
        cast_func(Mieru)
        cast_func(QUIC)
        cast_func(AnyTLS)
        cast_func(ShadowTLS)
        cast_func(Wireguard)
        cast_func(Tailscale)
        cast_func(SSH)
        cast_func(Tor)
        cast_func(Custom)
        cast_func(ExtraCore)

        #undef cast_func
    };


    class OldProxyEntityCompat : public JsonStore{
        public:
        virtual ConfJsMap _map()  override;
        std::shared_ptr<Configs::ProxyEntity> proxy;
        OldProxyEntityCompat(std::shared_ptr<Configs::ProxyEntity> b): proxy(b){};
    };


    class OldBeanEntityCompat : public JsonStore{
        public:
        virtual ConfJsMap _map()  override;
        std::shared_ptr<Configs::AbstractBean> bean;
        OldBeanEntityCompat(std::shared_ptr<Configs::AbstractBean> b): bean(b){};
    };
} // namespace Configs
