#include "include/global/Configs.hpp"
#include "include/configs/proxy/Preset.hpp"
#include "include/sys/Settings.h"

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
#include <include/api/RPC.h>

#include <include/js/version.h>
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#else
#ifdef Q_OS_UNIX
#include <include/sys/linux/LinuxCap.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace Configs_ConfigItem {

    void JsonStore::_put(QMap<QString, std::shared_ptr<configItem>> & items, 
            QString str, void* p, itemType type
    ){
        auto item = std::make_shared<configItem>(str, (size_t)((size_t)p - (size_t)(void*)this), type);
        items[str] = item;
    };
    
    QString JsonStore::_name(void *p) {
        for (const auto &_item: _map()) {
            if ((_item->ptr + this) == p) return _item->name;
        }
        return {};
    }

    std::shared_ptr<configItem> JsonStore::_get(const QString &name) {
        // 直接 [] 会设置一个 nullptr ，所以先判断是否存在
        auto & _map = this->_map();
        if (_map.contains(name)) {
            return _map[name];
        }
        return nullptr;
    }
/*
    void JsonStore::_setValue(const QString &name, void *p) {
        auto item = _get(name);
        auto & _map = this->_map();
        if (item == nullptr) return;
        _map[name] = offsetof(this, p);
    }
*/

    QJsonObject JsonStore::ToJson(const QStringList &without) {
        QJsonObject object;
        auto & _map = this->_map();
        for (const auto &_item: _map) {
            auto item = _item.get();
            if (item == nullptr){
                continue;
            }
            QString name = item->name;
            if (without.contains(name)) continue;

            void * ptr = (void*)(((size_t)(void*)this) + item->ptr);
            switch (item->type) {
                case itemType::string:
                    // Allow Empty
                    object.insert(name, *(QString *) ptr);
                    break;
                case itemType::integer:
                    object.insert(name, *(int *) ptr);
                    break;
                case itemType::integer64:
                    object.insert(name, *(long long *) ptr);
                    break;
                case itemType::boolean:
                    object.insert(name, *(bool *) ptr);
                    break;
                case itemType::stringList: {
                    auto jsonarray = QListStr2QJsonArray(*(QList<QString> *) ptr);
                    if (jsonarray.isEmpty()) continue;
                    object.insert(name, jsonarray);
                    break;
                }
                case itemType::integerList: {
                    auto jsonarray = QListInt2QJsonArray(*(QList<int> *) ptr);
                    if (jsonarray.isEmpty()) continue;
                    object.insert(name, jsonarray);
                    break;
                }
                case itemType::jsonStore:
                    {
                    //  的指针
                        JsonStore * store = *(JsonStore**)ptr;
                        if (store != nullptr){
                            object.insert(name, store->ToJson());
                        }
                    } 
                    break;
                case itemType::jsonStoreList:
                    QJsonArray jsonArray;
                    auto arr = *(QList<JsonStore*> *) ptr;
                    for ( JsonStore* obj : arr) {
                        if (obj ==  nullptr){
                            continue;
                        }
                        jsonArray.push_back(obj->ToJson());
                    }
                    object.insert(name, jsonArray);
                    break;
            }
        }
        return object;
    }

    QByteArray JsonStore::ToJsonBytes() {
        QJsonDocument document;
        document.setObject(ToJson());
        return document.toJson(save_control_compact ? QJsonDocument::Compact : QJsonDocument::Indented);
    }

    void * configItem::getPtr(void * p){
        return (void*)((size_t)p + ptr);
    }

    void JsonStore::FromJson(QJsonObject object) {
        auto & _map = this->_map();
        for (const auto &key: object.keys()) {
            if (_map.count(key) == 0) {
                continue;
            }
            auto value = object[key];
            auto item = _map[key].get();

            if (item == nullptr)
                continue; // 故意忽略

            auto ptr = (void*)(((size_t)(void*)this) + item->ptr);
            switch (item->type) {
                case itemType::string:
                    if (value.type() != QJsonValue::String) {
                        continue;
                    }
                    *(QString *) ptr = value.toString();
                    break;
                case itemType::integer:
                    if (value.type() != QJsonValue::Double) {
                        continue;
                    }
                    *(int *) ptr = value.toInt();
                    break;
                case itemType::integer64:
                    if (value.type() != QJsonValue::Double) {
                        continue;
                    }
                    *(long long *) ptr = value.toDouble();
                    break;
                case itemType::boolean:
                    if (value.type() != QJsonValue::Bool) {
                        continue;
                    }
                    *(bool *) ptr = value.toBool();
                    break;
                case itemType::stringList:
                    if (value.type() != QJsonValue::Array) {
                        continue;
                    }
                    *(QList<QString> *) ptr = QJsonArray2QListString(value.toArray());
                    break;
                case itemType::integerList:
                    if (value.type() != QJsonValue::Array) {
                        continue;
                    }
                    *(QList<int> *) ptr = QJsonArray2QListInt(value.toArray());
                    break;
                case itemType::jsonStore:
                    if (value.type() != QJsonValue::Object) {
                        continue;
                    }
                    {
                        JsonStore * store = *(JsonStore**) ptr;
                        if (ptr == nullptr){
                            continue;
                        }
                        store->FromJson(value.toObject());
                    }
                    break;
                case itemType::jsonStoreList:
                    break;
            }
        }

        if (callback_after_load != nullptr) callback_after_load();
    }

    void JsonStore::_setValue(const QString &name, void *p) {
        auto item = _get(name);
        if (item == nullptr) return;

        void *ptr = (void*)((size_t)(void*)this + item->ptr);

        switch (item->type) {
            case itemType::string:
                *(QString *) ptr = *(QString *) p;
                break;
            case itemType::boolean:
                *(bool *) ptr = *(bool *) p;
                break;
            case itemType::integer:
                *(int *) ptr = *(int *) p;
                break;
            case itemType::integer64:
                *(long long *) ptr = *(long long *) p;
                break;
            // others...
            default:
                break;
        }
    }

    void JsonStore::FromJsonBytes(const QByteArray &data) {
        QJsonParseError error{};
        auto document = QJsonDocument::fromJson(data, &error);

        if (error.error != error.NoError) {
            qDebug() << "QJsonParseError" << error.errorString();
            return;
        }

        FromJson(document.object());
    }

    bool JsonStore::Save() {
        if (callback_before_save != nullptr) callback_before_save();
        if (save_control_no_save) return false;

        auto save_content = ToJsonBytes();
        auto save_content_hash = 
            QCryptographicHash::hash(save_content, QCryptographicHash::Sha3_224);
        auto changed = last_save_content != save_content_hash;
        last_save_content = save_content_hash;

        QFile file;
        file.setFileName(fn);
        if (file.open(QIODevice::ReadWrite | QIODevice::Truncate)){
            file.write(save_content);
        }
        file.close();

        return changed;
    }

    bool JsonStore::Load() {
        QFile file(fn);

        if (!file.exists()) {
            return false;
        }

        bool ok = file.open(QIODevice::ReadOnly);
        if (!ok) {
            if (load_control_must){
                MessageBoxWarning("error", "can not open config " + fn + "\n" + file.errorString());
            }
        } else {
            last_save_content = file.readAll();
            FromJsonBytes(last_save_content);
        }

        file.close();
        return ok;
    }

} // namespace Configs_ConfigItem

namespace Configs {

    DataStore *dataStore = new DataStore();

    // datastore

    DataStore::DataStore() : JsonStore() {

    }

    #define d_add(X, Y, B) _put(ptr, X, &this->Y, itemType::B)

    ConfJsMap & DataStore::_map(){
        static ConfJsMap ptr;
        static bool noninit = true;
        if (noninit){
        d_add("user_agent2", user_agent, string);
        d_add("test_url", test_latency_url, string);
        d_add("disable_tray", disable_tray, boolean);
        d_add("current_group", current_group, integer);
        d_add("inbound_address", inbound_address, string);
        d_add("inbound_socks_port", inbound_socks_port, integer);
        d_add("random_inbound_port", random_inbound_port, boolean);
        d_add("log_level", log_level, string);
        d_add("mux_protocol", mux_protocol, string);
        d_add("mux_concurrency", mux_concurrency, integer);
        d_add("mux_padding", mux_padding, boolean);
        d_add("mux_default_on", mux_default_on, boolean);
        d_add("test_concurrent", test_concurrent, integer);
   //     _add(new configItem("theme", &theme, itemType::string));
        d_add("custom_inbound", custom_inbound, string);
        d_add("custom_route", custom_route_global, string);
        d_add("net_skip_proxy", net_skip_proxy, boolean);
        d_add("remember_id", remember_id, integer);
        d_add("remember_enable", remember_enable, boolean);
   //     _add(new configItem("language", &language, itemType::integer));
   //     _add(new configItem("font", &font, itemType::string));
   //     _add(new configItem("font_size", &font_size, itemType::integer));
        d_add("spmode2", remember_spmode, stringList);
        d_add("skip_cert", skip_cert, boolean);
        d_add("hk_mw", hotkey_mainwindow, string);
        d_add("hk_group", hotkey_group, string);
        d_add("hk_route", hotkey_route, string);
        d_add("hk_spmenu", hotkey_system_proxy_menu, string);
        d_add("hk_toggle", hotkey_toggle_system_proxy, string);
        d_add("fakedns", fake_dns, boolean);
        d_add("active_routing", active_routing, string);
   //     _add(new configItem("mw_size", &mw_size, itemType::string));
        d_add("disable_traffic_stats", disable_traffic_stats, boolean);
        d_add("vpn_impl", vpn_implementation, string);
        d_add("vpn_mtu", vpn_mtu, integer);
        d_add("vpn_ipv6", vpn_ipv6, boolean);
        d_add("vpn_strict_route", vpn_strict_route, boolean);
        d_add("sub_clear", sub_clear, boolean);
        d_add("net_insecure", net_insecure, boolean);
        d_add("sub_auto_update", sub_auto_update, integer);
        d_add("sub_send_hwid", sub_send_hwid, boolean);
        d_add("start_minimal", start_minimal, boolean);
        d_add("max_log_line", max_log_line, integer);
        d_add("splitter_state", splitter_state, string);
        d_add("utlsFingerprint", utlsFingerprint, string);
        d_add("core_box_clash_api", core_box_clash_api, integer);
        d_add("core_box_clash_listen_addr", core_box_clash_listen_addr, string);
        d_add("core_box_clash_api_secret", core_box_clash_api_secret, string);
        d_add("core_box_underlying_dns", core_box_underlying_dns, string);
        d_add("enable_ntp", enable_ntp, boolean);
        d_add("ntp_server_address", ntp_server_address, string);
        d_add("ntp_server_port", ntp_server_port, integer);
        d_add("ntp_interval", ntp_interval, string);
        d_add("enable_dns_server", enable_dns_server, boolean);
        d_add("dns_server_listen_lan", dns_server_listen_lan, boolean);
        d_add("dns_server_listen_port", dns_server_listen_port, integer);
        d_add("dns_v4_resp", dns_v4_resp, string);
        d_add("dns_v6_resp", dns_v6_resp, string);
        d_add("dns_server_rules", dns_server_rules, stringList);
        d_add("enable_redirect", enable_redirect, boolean);
        d_add("redirect_listen_address", redirect_listen_address, string);
        d_add("redirect_listen_port", redirect_listen_port, integer);
        d_add("system_dns_set", system_dns_set, boolean);
        d_add("windows_set_admin", windows_set_admin, boolean);
        d_add("disable_win_admin", disable_run_admin, boolean);
        d_add("enable_stats", enable_stats, boolean);
        d_add("stats_tab", stats_tab, integer);
        d_add("proxy_scheme", proxy_scheme, string);
        d_add("disable_privilege_req", disable_privilege_req, boolean);
        d_add("enable_tun_routing", enable_tun_routing, boolean);
        d_add("speed_test_mode", speed_test_mode, integer);
        d_add("use_mozilla_certs", use_mozilla_certs, boolean);
        d_add("adblock_enable", adblock_enable, boolean);
        d_add("speedtest_timeout_ms", speed_test_timeout_ms, integer);
        d_add("urltest_timeout_ms", url_test_timeout_ms, integer);
        d_add("show_system_dns", show_system_dns, boolean);
        noninit = false;
        }
        return ptr;
    }

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

    #undef d_add
    #define d_add(X, Y, B) _put(ptr, X, &this->Y, itemType::B)

    // preset routing
    Routing::Routing(int preset) : JsonStore() {
        if (!Preset::SingBox::DomainStrategy.contains(domain_strategy)) domain_strategy = "";
        if (!Preset::SingBox::DomainStrategy.contains(outbound_domain_strategy)) outbound_domain_strategy = "";
    }

    ConfJsMap & Routing::_map(){
        static ConfJsMap ptr;
        static bool noninit = true;
        if (noninit){
        d_add("current_route_id", current_route_id, integer);
        d_add("remote_dns", remote_dns, string);
        d_add("remote_dns_strategy", remote_dns_strategy, string);
        d_add("direct_dns", direct_dns, string);
        d_add("direct_dns_strategy", direct_dns_strategy, string);
        d_add("domain_strategy", domain_strategy, string);
        d_add("outbound_domain_strategy", outbound_domain_strategy, string);
        d_add("sniffing_mode", sniffing_mode, integer);
        d_add("ruleset_mirror", ruleset_mirror, integer);
        d_add("use_dns_object", use_dns_object, boolean);
        d_add("dns_object", dns_object, string);
        d_add("dns_final_out", dns_final_out, string);
        noninit = false;
        }
        return ptr;
    }

    #undef d_add

    Shortcuts::Shortcuts() : JsonStore()
    {
    }
    ConfJsMap& Shortcuts::_map() {
        static ConfJsMap ptr;
        static bool noninit = true;
        if (noninit){
            _put(ptr, "keyval", &this->keyVal, itemType::stringList);
            noninit = false;
        }
        return ptr;
    }

    bool Shortcuts::Save()
    {
        keyVal.clear();
        for (auto [k, v] : asKeyValueRange(shortcuts))
        {
            if (v.isEmpty()) continue;
            keyVal << k << v.toString();
        }

        return JsonStore::Save();
    }

    bool Shortcuts::Load() {
        auto ret = JsonStore::Load();
        if (!ret) return false;
        if (keyVal.count()%2 != 0) return false;
        for (int i=0;i<keyVal.size();i+=2)
        {
            shortcuts[keyVal[i]] = QKeySequence(keyVal[i+1]);
        }
        return ret;
    }

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

    short isAdminCache = -1;

    bool isSetuidSet(const std::string& path) {
        return false;
    }

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
