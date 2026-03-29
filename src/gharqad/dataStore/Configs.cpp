#include <nekobox/dataStore/ConfigItem.hpp>
#include <nekobox/dataStore/Configs.hpp>
#include <nekobox/dataStore/DataStore.hpp>
#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/configs/proxy/AbstractBean.hpp>
#include <nekobox/sys/Settings.h>
#include <nekobox/global/keyvaluerange.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <memory>
#include <utility>
#include <nekobox/api/RPC.h>
#include <QBuffer>
#include <QVariantMap>
#include <nekobox/js/version.h>
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include "nekobox/sys/windows/guihelper.h"
#else
#ifdef Q_OS_UNIX
#include <nekobox/sys/linux/LinuxCap.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif


namespace Configs_ConfigItem {


         QString JsonStore::_name(void *p){
            for (auto & item: _map()){
                if (item->getPtr(this) == p){
                    return item->name;
                }
            }
            return "";
        };

    std::shared_ptr<configItem> JsonStore::_get(const QString &name) {
        return std::static_pointer_cast<configItem>(this->_get_const_job(name));
    }

    std::shared_ptr<const configItem> JsonStore::_get_const(const QString &name) const {
        return std::static_pointer_cast<const configItem>(this->_get_const_job(name));
    }

    std::shared_ptr<configItem> JsonStore::_get_const_job(const QString &name) const {
        JsonStore * store = (JsonStore*) this;
        auto _map = store->_map();
        auto h = Configs::hash(name);
        if (_map.contains(h)) {
            auto ret =  _map.value(h);
            if (ret->name == name){
                return ret;
            }
        }
        return nullptr;
    }

    QJsonObject JsonStore::ToJson(const QStringList &without) const {
        QJsonObject object;
        JsonStore * store = (JsonStore*) this;
        auto _map = store->_map();
        for (const auto &_item: _map.values()) {
            auto item = _item.get();
            if (item == nullptr){
                continue;
            }
            const QString & name = item->name;
            if (without.contains(name)) continue;

            object.insert(name, item->getNode(store));
        }
        return object;
    }

    QByteArray JsonStore::ToJsonBytes(const QStringList &without) const {
        return QJsonObject2QString(ToJson(without), false).toUtf8();
    }

    void * configItem::getPtr(const JsonStore * p) const {
        return (void*)((size_t)p + ptr);
    }

    void JsonStore::FromJson(QJsonObject object) {
        auto  _map = this->_map();
        for (const auto &key: object.keys()) {
            auto h = Configs::hash(key);
            QJsonValue value;
            if (_map.count(h) == 0) {
                if (UnknownKeyHash(h) && ((value = object[key]).isObject())){
                    this->FromJson(value.toObject());
                }
                continue;
            } else {
                value = object[key];
            }
            auto item = _map.value(h).get();

            if ((item == nullptr) || (item->name != key)){
                continue;
            }

            item->setNode(this, value);

        }

//        if (callback_after_load != nullptr) callback_after_load();
    }

    void JsonStore::_setValue(const JsonStore * store, const void *p){
        for (auto & item: ((JsonStore*)store)->_map()){
            if (item->getPtr(store) == p){
                this->_setValue(item->name, item->getNode((JsonStore*)store));
            }
        }
    }

    QJsonValue JsonStore::_getValue(const QString & name) const {
        auto store = (JsonStore *) (this);
        auto item = store->_get(name);
        return item->getNode(store);
    }

    void JsonStore::_setValue(const QString &name, const QJsonValue & node) {
        auto item = _get(name);
        if (item == nullptr) return;

        item->setNode(this, node);
    }

    void JsonStore::FromJsonBytes(const QByteArray &data) {
        QJsonParseError error{};
        auto document = QJsonDocument::fromJson(data, &error);

        if (error.error != error.NoError) {
            #ifdef DEBUG_MODE
            qDebug() << "QJsonParseError" << error.errorString();
            #endif
            return;
        }

        FromJson(document.object());
    }

    QByteArray JsonStore::content(){
        bool force_json_configs = Configs::ForceJsonConfigs ;

        return (force_json_configs) ? this->ToJsonBytes() : this->ToBytes({}, true);
    }


    void JsonStore::content(const QByteArray & byteArray){
        if (byteArray.size() > 7) {
            QByteArray magic = byteArray.first(7);
#ifdef DEBUG_MODE
            qDebug() << "magic is: " << magic << (magic == "NekoBox");
#endif
            if (magic == "NekoBox"){
                FromBytes(byteArray.mid(7));
            } else {
                goto is_json;
            };
        } else {
            is_json:
            FromJsonBytes(byteArray);
        }
    }

    bool JsonStore::SaveToFile(const QString & file){
        return WriteFile(file, content());
    }

    bool JsonStore::LoadFromFile(const QString & fn){
        QFile file(fn);

        if (!file.exists()) {
            return false;
        }
        bool ok = file.open(QIODevice::ReadOnly);
        if (!ok) {
            #ifdef DEBUG_MODE
    //        if (load_control_must){
                qDebug() << ("can not open config " + fn + "\n" + file.errorString());
            #endif
        } else {
            content(file.readAll());
        }
  //      l2:
        file.close();
        return ok;
    }

    bool JsonStore::Save() {
    //    if (callback_before_save != nullptr) callback_before_save();
        if (save_control_no_save() || (this->StoreType() == Configs::JsonStoreType::NoSave)) {
#ifdef DEBUG_MODE
            qDebug() << "Store File Name Is" << Configs::getJsonStoreFileName(this->StoreType(), this->Id());
#endif
            return false;
        }
        auto ret = Configs::databaseManager->Save(this);
        if (ret){
            this->storage_exists(true);
        }
        return ret;
    }

    bool JsonStore::Load() {
        if (Configs::JsonStoreType::NoSave == this->StoreType()){
            return false;
        }
        #ifdef DEBUG_MODE
            qDebug() << "Database Manager IS: " << (long long)(void*)Configs::databaseManager.get();
        #endif
        auto ret = Configs::databaseManager->Load(this);
        if (ret){
            this->storage_exists(true);
        }
        return ret;
    }

    JsonEnum& JsonEnum::set(int value){
#ifdef DEBUG_MODE
            qDebug() << "ENUM IS SETTING" << value;
#endif
        this->value = value;
        return *this;
    }
    JsonEnum& JsonEnum::set(const QString& value){

#ifdef DEBUG_MODE
            qDebug() << " add string (=) " << value;
#endif
        auto map = _map();

#ifdef DEBUG_MODE
            qDebug() << "ENUM IS SETTING" << value;
#endif
        try{
            this->value = map.left.at(value.toStdString());
        } catch (std::out_of_range){
#ifdef DEBUG_MODE
            qDebug() << "ENUM NOT FOUND" << value;
#endif
            this->value = 0;
        }

#ifdef DEBUG_MODE
            qDebug() << "ENUM IS SET" << this->value;
#endif
        return *this;
    }
    JsonEnum& JsonEnum::set(const char* value){
        this->set(QString(value));
        return *this;
    }
    JsonEnum& JsonEnum::set(const QByteArray& value){
        int val;
        if (value[0] == '\0'){
            memcpy(&val, value.constData() + 1, sizeof(val));
            this->value = val;
        } else {
            this->set(QString::fromUtf8(value));
        }
        return *this;
    }
    JsonEnum& JsonEnum::set(const QJsonValue& val){
        if (val.isString()){
            this->set(val.toString());
        } else if (val.isDouble()){
            this->set(val.toInt());
        }
        return *this;
    }
    JsonEnum::operator QJsonValue() const {
        return (QString)(*this);
    }
    JsonEnum::operator int() const {
        return this->value;
    }
    JsonEnum::operator QString() const {
        auto map = _map();
        try {
            return map.right.at(this->value).get_name();
        } catch (std::out_of_range){
#ifdef DEBUG_MODE
            qDebug() << this->value << "NOT FOUND";
#endif
            return "";
        }
    }
    JsonEnum::operator QByteArray() const {
        QByteArray arr;
        arr.append('\0');  // zero prefix
        arr.append(reinterpret_cast<const char*>(&value), sizeof(value));
        return arr;
    }
    const boost::bimap<EnumFieldName, int>& JsonEnum::_map() const {
#ifdef DEBUG_MODE
        qDebug() << "NULL BIMAP";
#endif
        static boost::bimap<EnumFieldName, int> m;
        static bool initialized = false;
        return m;
    }

} // namespace Configs_ConfigItem

namespace Configs {

QByteArray hash(const QString & input)
{
QByteArray hash = QCryptographicHash::hash(
    input.toUtf8(),
    QCryptographicHash::Md5
);
  Q_ASSERT(hash.size() == 16);
  return hash;
}

    bool ForceJsonConfigs = 
    #ifdef DEBUG_MODE
    true
    #else
    false
    #endif
    ;

    DataStore *dataStore = new DataStore();

    // datastore

    DataStore::DataStore() : JsonStore() {
    }

    DECL_MAP(DataStore)
        ADD_MAP("sub_custom_hwid_params", sub_custom_hwid_params, string);
        ADD_MAP("user_agent2", user_agent, string);
        ADD_MAP("test_url", test_latency_url, string);
        ADD_MAP("disable_tray", disable_tray, boolean);
        ADD_MAP("current_group", current_group, integer);
        ADD_MAP("inbound_address", inbound_address, string);
        ADD_MAP("inbound_socks_port", inbound_socks_port, integer);
        ADD_MAP("random_inbound_port", random_inbound_port, boolean);
        ADD_MAP("log_level", log_level, string);
        ADD_MAP("mux_protocol", mux_protocol, string);
        ADD_MAP("mux_concurrency", mux_concurrency, integer);
        ADD_MAP("mux_padding", mux_padding, boolean);
        ADD_MAP("mux_default_on", mux_default_on, boolean);
        ADD_MAP("test_concurrent", test_concurrent, integer);
    //    ADD_MAP("ruleset_json_url", ruleset_json_url, string);
   //     _add(new configItem("theme", &theme, itemType::string));
        ADD_MAP("inbound_username", inbound_username, string);
        ADD_MAP("inbound_password", inbound_password, string);

        ADD_MAP("custom_inbound", custom_inbound, string);
        ADD_MAP("custom_route", custom_route_global, string);
        ADD_MAP("network_use_proxy", net_use_proxy, boolean);
        ADD_MAP("remember_id", remember_id, integer);
        ADD_MAP("remember_enable", remember_enable, boolean);
   //     _add(new configItem("language", &language, itemType::integer));
   //     _add(new configItem("font", &font, itemType::string));
   //     _add(new configItem("font_size", &font_size, itemType::integer));
        ADD_MAP("spmode2", remember_spmode, stringList);
        ADD_MAP("skip_cert", skip_cert, boolean);
        ADD_MAP("hk_mw", hotkey_mainwindow, string);
        ADD_MAP("hk_group", hotkey_group, string);
        ADD_MAP("hk_route", hotkey_route, string);
        ADD_MAP("hk_spmenu", hotkey_system_proxy_menu, string);
        ADD_MAP("hk_toggle", hotkey_toggle_system_proxy, string);
        ADD_MAP("fakedns", fake_dns, boolean);
        ADD_MAP("active_routing", active_routing, string);
   //     _add(new configItem("mw_size", &mw_size, itemType::string));
        ADD_MAP("disable_traffic_stats", disable_traffic_stats, boolean);
        ADD_MAP("vpn_stack", vpn_implementation, string);
        ADD_MAP("vpn_mtu", vpn_mtu, integer);
        ADD_MAP("vpn_ipv6", vpn_ipv6, boolean);
        ADD_MAP("vpn_strict_route", vpn_strict_route, boolean);
        ADD_MAP("sub_clear", sub_clear, boolean);
        
        ADD_MAP("sub_rm_invalid", sub_rm_invalid, boolean);
        ADD_MAP("sub_url_test", sub_url_test, boolean);
        ADD_MAP("sub_rm_duplicates", sub_rm_duplicates, boolean);
        ADD_MAP("sub_rm_unavailable", sub_rm_unavailable, boolean);

        ADD_MAP("net_insecure", net_insecure, boolean);
        ADD_MAP("sub_auto_update", sub_auto_update, integer);
        ADD_MAP("sub_send_hwid", sub_send_hwid, boolean);
   //     ADD_MAP("start_minimal", start_minimal, boolean);
   //     ADD_MAP("max_log_line", max_log_line, integer);
   //     ADD_MAP("splitter_state", splitter_state, string);
        ADD_MAP("utlsFingerprint", utlsFingerprint, string);
        ADD_MAP("core_box_clash_api", core_box_clash_api, integer);
        ADD_MAP("core_box_clash_listen_addr", core_box_clash_listen_addr, string);
        ADD_MAP("core_box_clash_api_secret", core_box_clash_api_secret, string);
        ADD_MAP("core_box_underlying_dns", core_box_underlying_dns, string);
        ADD_MAP("enable_ntp", enable_ntp, boolean);
        ADD_MAP("ntp_server_address", ntp_server_address, string);
        ADD_MAP("ntp_server_port", ntp_server_port, integer);
        ADD_MAP("ntp_interval", ntp_interval, string);
        ADD_MAP("enable_dns_server", enable_dns_server, boolean);
        ADD_MAP("dns_server_listen_lan", dns_server_listen_lan, boolean);
        ADD_MAP("dns_server_listen_port", dns_server_listen_port, integer);
        ADD_MAP("dns_v4_resp", dns_v4_resp, string);
        ADD_MAP("dns_v6_resp", dns_v6_resp, string);
        ADD_MAP("dns_server_rules", dns_server_rules, stringList);
        ADD_MAP("enable_redirect", enable_redirect, boolean);
        ADD_MAP("redirect_listen_address", redirect_listen_address, string);
        ADD_MAP("redirect_listen_port", redirect_listen_port, integer);
        ADD_MAP("system_dns_set", system_dns_set, boolean);
        ADD_MAP("windows_set_admin", windows_set_admin, boolean);
        ADD_MAP("disable_win_admin", windows_no_admin, boolean);
        ADD_MAP("enable_stats", connection_statistics, boolean);
        ADD_MAP("stats_tab", stats_tab, integer);
        ADD_MAP("proxy_scheme", proxy_scheme, string);
        ADD_MAP("disable_privilege_req", disable_privilege_req, boolean);
        ADD_MAP("enable_tun_routing", enable_tun_routing, boolean);
        ADD_MAP("speed_test_mode", speed_test_mode, integer);
        ADD_MAP("use_mozilla_certs", use_mozilla_certs, boolean);
        ADD_MAP("adblock_enable", adblock_enable, boolean);
        ADD_MAP("speedtest_timeout_ms", speed_test_timeout_ms, integer);
        ADD_MAP("urltest_timeout_ms", url_test_timeout_ms, integer);
        ADD_MAP("show_system_dns", show_system_dns, boolean);
  //      ADD_MAP("cache_database_name", cache_database, string);
        ADD_MAP("simple_dl_url", simple_dl_url, string);
        ADD_MAP("use_json_configs", force_json_configs, booleanPtr);
        ADD_MAP("auto_test_enable", auto_test_enable, boolean);
        ADD_MAP("auto_test_interval_seconds", auto_test_interval_seconds, integer);
        ADD_MAP("auto_test_proxy_count", auto_test_proxy_count, integer);
        ADD_MAP("auto_test_working_pool_size", auto_test_working_pool_size, integer);
        ADD_MAP("auto_test_latency_threshold_ms", auto_test_latency_threshold_ms, integer);
        ADD_MAP("auto_test_failure_retry_count", auto_test_failure_retry_count, integer);
        ADD_MAP("auto_test_target_url", auto_test_target_url, string);
        ADD_MAP("auto_test_tun_failover", auto_test_tun_failover, boolean);
  //      ADD_MAP("auto_redirect", auto_redirect, boolean);
        ADD_MAP("route_exclude_addrs", route_exclude_addrs, stringList);
        ADD_MAP("tun_address", tun_address, string);
        ADD_MAP("tun_address_6", tun_address_6, string);
    STOP_MAP

    void DataStore::UpdateStartedId(int id) {
        started_id = id;
        remember_id = id;
        Save();
    }

    QString DataStore::GetUserAgent(bool isDefault) const {
        if (user_agent.isEmpty()) {
            isDefault = true;
        }
        if (isDefault) {
            QString version = SubStrBefore(NKR_VERSION, "-");
            if (!version.contains(".")) version = "1.0.0";
            return "nekobox/" + version + " (Prefer ClashMeta Format)";
        }
        return user_agent;
    }

    // preset routing
    Routing::Routing(int preset) : JsonStore() {
        if (!Preset::SingBox::DomainStrategy.contains(domain_strategy)) domain_strategy = "";
        if (!Preset::SingBox::DomainStrategy.contains(outbound_domain_strategy)) outbound_domain_strategy = "";
    }

    DECL_MAP(Routing)
        ADD_MAP("current_route_id", current_route_id, integer);
        ADD_MAP("remote_dns", remote_dns, string);
        ADD_MAP("remote_dns_strategy", remote_dns_strategy, string);
        ADD_MAP("direct_dns", direct_dns, string);
        ADD_MAP("direct_dns_strategy", direct_dns_strategy, string);
        ADD_MAP("domain_strategy", domain_strategy, string);
        ADD_MAP("outbound_domain_strategy", outbound_domain_strategy, string);
        ADD_MAP("sniffing_mode", sniffing_mode, integer);
        ADD_MAP("ruleset_mirror", ruleset_mirror, integer);
        ADD_MAP("ruleset_json_url", ruleset_json_url, string);
        ADD_MAP("use_dns_object", use_dns_object, boolean);
        ADD_MAP("dns_object", dns_object, string);
        ADD_MAP("dns_final_out_direct", dns_final_out_direct, boolean);
    STOP_MAP

    #undef d_add



    QStringList Routing::List() {
        return {"Default"};
    }

    // System Utils

    QString FindCoreRealPath() {
        auto fn = getCorePath();
        auto fi = QFileInfo(fn);
        if (fi.isSymLink()) return fi.symLinkTarget();
        return fn;
    }

    bool isSetuidSet(const std::string& path) {
        return false;
    }

    static signed char isAdminCache = -1;


    bool IsAdmin(bool forceRenew) {
        if (isAdminCache >= 0 && !forceRenew) return isAdminCache;

        bool admin = false;
#ifdef Q_OS_WIN
#ifdef EXIT_IF_UAC_REQUIRED
        admin = Windows_IsInAdmin();
        dataStore->windows_set_admin = admin;
#define SKIP_ASK_CORE
#endif
#endif 

#ifndef SKIP_ASK_CORE
        bool ok;
        auto isPrivileged = API::defaultClient->IsPrivileged(&ok);
        admin = ok && isPrivileged;
#endif
        isAdminCache = admin;
        return admin;
    };
    QString GetBasePath() {
        return QDir::currentPath();
    }

} // namespace Configs
