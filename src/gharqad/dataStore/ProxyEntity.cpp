#ifdef _WIN32
#include <winsock2.h>
#endif

#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/dataStore/Utils.hpp>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <qnamespace.h>
#include <QCoreApplication>
#include <nekobox/configs/proxy/includes.h>
#include <nekobox/dataStore/Database.hpp>


namespace Configs
{


[[nodiscard]]  QString ProxyEntity::DisplayAddress(){
    return (type != "chain" && type != "custom" && type != "extracore" &&
            type != "tailscale" && type != "tor") ? serverAddress + ":" + QString::number(serverPort) : "";
}

[[nodiscard]]  QString ProxyEntity::DisplayName(){
    return name;
}

[[nodiscard]]  QString ProxyEntity::DisplayCoreType(){
    return software_core_name;
}

[[nodiscard]]  QString ProxyEntity::DisplayType(){
    return ProfileManager::GetDisplayType(this->type);
}

[[nodiscard]]  QString ProxyEntity::DisplayTypeAndName(){
    return "[" + DisplayType() + "]" + name;
}

    #define _add(map1, X, Y, B) _put(map1, X, &this->Y)
    //, ITEM_TYPE(B))

    QByteArray bean_key = Configs::hash("bean");

    bool ProxyEntity::UnknownKeyHash(const QByteArray & array) {
#ifdef DEBUG_MODE
    qDebug()<<"Invalid Bean";
#endif
        if (array == bean_key){
            this->same_path_for_bean(true);
            return true;
        }
        return false;
    };

    bool ProxyEntity::isValid() const {
        return Preset::SingBox::OutboundTypes.contains(this->type);
    }

    void ProxyEntity::ResetBeans(){
        this->weak_bean.reset();
        this->strong_bean.reset();
    }

    bool ProxyEntity::Save(){
        return SavePrivate();
    }

    bool ProxyEntity::SavePrivate() {
        std::shared_ptr<Configs::AbstractBean> bean = this->weak_bean.lock();
        if (bean != nullptr){
                bean->save_control_no_save(this->save_control_no_save());
                bean->Save();
                #ifdef DEBUG_MODE
                    qDebug() << "Save Bean Entity Id" << id << "of type" << bean->type() << "and json" << bean->ToJson();
                #endif
                this->strong_bean.reset();
        }
        return JsonStore::Save();
    }

    #ifdef DEBUG_MODE
        #define cast(X) } else if (type == #X) { qDebug() << "Proxy Type is " << #X ;
    #else
        #define cast(X) } else if (type == #X) {
    #endif

    std::shared_ptr<const Configs::AbstractBean> ProxyEntity::bean() const {
        auto ent = (ProxyEntity*)this;
        Configs::AbstractBean * bean;
        {
            std::shared_ptr<AbstractBean> ret1 = nullptr;
            if (ent->weak_bean.use_count() > 0){
                ret1 = ent->weak_bean.lock();
            }
            if (ret1 != nullptr){
                return ret1;
            }
        }
        bool make_strong_bean = false;

        if (type == ""){
          goto unknown_type;
        cast(socks)
            bean = new Configs::SocksBean(ent, Configs::SocksBean::type_Socks5);
        cast(mieru)
            bean = new Configs::MieruBean(ent);  
        cast(http)
            bean = new Configs::HttpBean(ent);
        cast(shadowsocks)
            bean = new Configs::ShadowSocksBean(ent);
        cast(chain)
            bean = new Configs::ChainBean(ent);
        cast(vmess)
            bean = new Configs::VMessBean(ent);
        cast(trojan)
            bean = new Configs::TrojanVLESSBean(ent, Configs::TrojanVLESSBean::proxy_Trojan);
        cast(vless)
            bean = new Configs::TrojanVLESSBean(ent, Configs::TrojanVLESSBean::proxy_VLESS);
        cast(hysteria)
            bean = new Configs::QUICBean(ent, Configs::QUICBean::proxy_Hysteria);
        cast(hysteria2)
            bean = new Configs::QUICBean(ent, Configs::QUICBean::proxy_Hysteria2);
        cast(tuic)
            bean = new Configs::QUICBean(ent, Configs::QUICBean::proxy_TUIC);
        cast(anytls)
            bean = new Configs::AnyTLSBean(ent);
        cast(shadowtls)
            bean = new Configs::ShadowTLSBean(ent);
        cast(wireguard) 
            bean = new Configs::WireguardBean(ent);
        cast(tailscale)
            bean = new Configs::TailscaleBean(ent);
        cast(ssh) 
            bean = new Configs::SSHBean(ent);
        cast(custom)
            bean = new Configs::CustomBean(ent);
        cast(extracore)
            bean = new Configs::ExtraCoreBean(ent);
        cast(tor)
            bean = new Configs::TorBean(ent);
        cast(naive)
            bean = new Configs::NaiveBean(ent);
        cast(trusttunnel)
            bean = new Configs::TrustTunnelBean(ent);
        cast(juicity)
            bean = new Configs::JuicityBean(ent);
        } else {
            unknown_type:
            bean = new Configs::AbstractBean(ent, -114514);
            make_strong_bean = true;
        }

        std::shared_ptr<Configs::AbstractBean> ret(bean);
#ifdef DEBUG_MODE
qDebug() << "Type Unknown" << make_strong_bean;
#endif

        if (!make_strong_bean){
            if (this->id > -1){
                ret->Load();
                ret->save_control_no_save(true);
            } else {
                make_strong_bean = true;
            }
        }
        {
            ProxyEntity * entity = (ProxyEntity*)this;
            entity->weak_bean = ret;
            if (make_strong_bean){
                entity->strong_bean = ret;
                #ifdef DEBUG_MODE
                qDebug() << "Strong Bean Set" << this->strong_bean.use_count() << entity->strong_bean.use_count();
                #endif
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

    ProxyEntity::~ProxyEntity() {
        this->strong_bean.reset();
        auto weak = this->weak_bean.lock();
        if (!save_control_no_save() && (weak != nullptr)){
            SavePrivate();
            weak->entity = nullptr;
            weak.reset();
        }
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
            std::shared_ptr<const Configs::X##Bean> ret = std::static_pointer_cast<const Configs::X##Bean>(bean());    \
            return ret; \
        };

        cast_func(Chain)
        cast_func(Http)
        cast_func(Socks)
        cast_func(ShadowSocks)
        cast_func(VMess)
        cast_func(TrojanVLESS)
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
        cast_func(Naive)
        cast_func(TrustTunnel)
        cast_func(Juicity)

}
