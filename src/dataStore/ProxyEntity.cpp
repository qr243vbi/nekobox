#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <qnamespace.h>
#include <QCoreApplication>
#include <nekobox/configs/proxy/includes.h>

namespace Configs
{

    #define _add(map1, X, Y, B) _put(map1, X, &this->Y)
    //, ITEM_TYPE(B))

    static QByteArray bean_key = Configs::hash("bean");

    void ProxyEntity::UnknownKeyHash(const QByteArray & array) {
        if (array == bean_key){
            invalid = true;
        }
    };

    bool ProxyEntity::Save() {
        auto bean = this->weak_bean.lock();
        if (bean != nullptr){
            if (!this->bean_cfg.isEmpty()){
                bean->save_control_no_save = this->save_control_no_save;
                bean->fn = this->bean_cfg;
                bean->Save();
                this->strong_bean.reset();
                bean.reset();
            }
        }
        return JsonStore::Save();
    }

    std::shared_ptr<const Configs::AbstractBean> ProxyEntity::bean() const {
        auto ent = (ProxyEntity*)this;
        Configs::AbstractBean * bean;
        {
            auto ret1 = this->weak_bean.lock();
            if (ret1 != nullptr){
                return ret1;
            }
        }
        bool skip_load = false;

        if (type == "socks") {
            bean = new Configs::SocksHttpBean(ent, Configs::SocksHttpBean::type_Socks5);
        } else if (type == "mieru"){
            bean = new Configs::MieruBean(ent);  
        } else if (type == "http") {
            bean = new Configs::SocksHttpBean(ent, Configs::SocksHttpBean::type_HTTP);
        } else if (type == "shadowsocks") {
            bean = new Configs::ShadowSocksBean(ent);
        } else if (type == "chain") {
            bean = new Configs::ChainBean(ent);
        } else if (type == "vmess") {
            bean = new Configs::VMessBean(ent);
        } else if (type == "trojan") {
            bean = new Configs::TrojanVLESSBean(ent, Configs::TrojanVLESSBean::proxy_Trojan);
        } else if (type == "vless") {
            bean = new Configs::TrojanVLESSBean(ent, Configs::TrojanVLESSBean::proxy_VLESS);
        } else if (type == "hysteria") {
            bean = new Configs::QUICBean(ent, Configs::QUICBean::proxy_Hysteria);
        } else if (type == "hysteria2") {
            bean = new Configs::QUICBean(ent, Configs::QUICBean::proxy_Hysteria2);
        } else if (type == "tuic") {
            bean = new Configs::QUICBean(ent, Configs::QUICBean::proxy_TUIC);
        } else if (type == "anytls") {
            bean = new Configs::AnyTLSBean(ent);
        } else if (type == "shadowtls") {
            bean = new Configs::ShadowTLSBean(ent);
        } else if (type == "wireguard") {
            bean = new Configs::WireguardBean(ent);
        } else if (type == "tailscale") {
            bean = new Configs::TailscaleBean(ent);
        } else if (type == "ssh") {
            bean = new Configs::SSHBean(ent);
        } else if (type == "custom") {
            bean = new Configs::CustomBean(ent);
        } else if (type == "extracore") {
            bean = new Configs::ExtraCoreBean(ent);
        } else if (type == "tor"){
            bean = new Configs::TorBean(ent);
        } else {
            bean = new Configs::AbstractBean(ent, -114514);
            skip_load = true;
        }
        bool make_strong_bean = false;
        std::shared_ptr<Configs::AbstractBean> ret(bean);
        if (!this->bean_cfg.isEmpty() && !skip_load){
            ret->fn = this->bean_cfg;
            ret->Load();
        } else {
            make_strong_bean = true;
        }
        {
            ProxyEntity * entity = (ProxyEntity*)this;
            entity->weak_bean = ret;
            if (make_strong_bean){
                entity->strong_bean = ret;
            }
            return ret;
        }
    }

    ConfJsMap ProxyEntity::_map() {
        static ConfJsMapStat map1;
        static ConfJsMapStat map2;
        static bool map1_init = true;
        if (map1_init ){
            map1_init = false;
            _add(map1, "type", type, string);
            _add(map1, "id", id, integer);
            _add(map1, "gid", gid, integer);
            _add(map1, "yc", latencyInt, integer);
            _add(map1, "dl", dl_speed, string);
            _add(map1, "ul", ul_speed, string);
            _add(map1, "report", full_test_report, string);
            _add(map1, "country", test_country, string);
            _add(map1, "is_working", is_working, boolean);
            _add(map1, "last_auto_test_time", last_auto_test_time, integer64);
            _add(map1, "name", name, string);
            _add(map1, "addr", serverAddress, string);
            _add(map1, "port", serverPort, integer);
            
     //       map2.insert(map1);
     //       _add(map2, "bean", bean, jsonStore);
            _add(map1, "traffic", traffic_data, jsonStore);
        }
        #undef _add
        
     //   if (bean.get() == nullptr){
            return map1;
     //   } else {
    //        return map2;
    //    }
    }

    ProxyEntity::ProxyEntity(const QString &type_) {
        if (type_ != nullptr) this->type = type_;
    };

    QString ProxyEntity::DisplayTestResult() const {
        QString result;
        if (latencyInt < 0) {
            result = QCoreApplication::translate("MainWindow", "Unavailable");
        } else if (latencyInt > 0) {
            if (!test_country.isEmpty()) result += UNICODE_LRO + CountryCodeToFlag(test_country) + " ";
            result += QString("%1 ms").arg(latencyInt);
        }
        if (!dl_speed.isEmpty() && dl_speed != "N/A") result += " ↓" + dl_speed;
        if (!ul_speed.isEmpty() && ul_speed != "N/A") result += " ↑" + ul_speed;
        return result;
    }



        #define cast_func(X)         \
        std::shared_ptr<const Configs::X##Bean> ProxyEntity::X##Bean() const {           \
            std::shared_ptr<const Configs::X##Bean> ret = std::dynamic_pointer_cast<const Configs::X##Bean>(bean());    \
            return ret; \
        };

        cast_func(Chain)
        cast_func(SocksHttp)
        cast_func(ShadowSocks)
        cast_func(VMess)
        cast_func(TrojanVLESS)
  //      cast_func(Naive)
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

}
