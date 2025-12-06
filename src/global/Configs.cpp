#include "include/global/Configs.hpp"
#include "include/configs/proxy/Preset.hpp"
#include "include/configs/proxy/AbstractBean.hpp"
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

    void JsonStore::_put(ConfJsMap items, 
            QString str, void* p, itemType type
    ){
        auto item = std::make_shared<configItem>(str, (size_t)((size_t)p - (size_t)(void*)this), type);
        items.insert(str, item);
    };

         QString JsonStore::_name(void *p){
            size_t ptr = ((size_t)(p) - (size_t) (this));
            for (auto & item: _map()){
                if (item->ptr == ptr){
                    return item->name;
                }
            }
            return "";
        };

    std::shared_ptr<configItem> JsonStore::_get(const QString &name) {
        // 直接 [] 会设置一个 nullptr ，所以先判断是否存在
        auto _map = this->_map();
        if (_map.contains(name)) {
            return _map.value(name);
        }
        return nullptr;
    }

    QJsonObject JsonStore::ToJson(const QStringList &without) {
        QJsonObject object;
        auto _map = this->_map();
        for (const auto &_item: _map.values()) {
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
        auto  _map = this->_map();
        for (const auto &key: object.keys()) {
            if (_map.count(key) == 0) {
                continue;
            }
            auto value = object[key];
            auto item = _map.value(key).get();

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

    DECL_MAP(DataStore)
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
        ADD_MAP("ruleset_json_url", ruleset_json_url, string);
   //     _add(new configItem("theme", &theme, itemType::string));
        ADD_MAP("custom_inbound", custom_inbound, string);
        ADD_MAP("custom_route", custom_route_global, string);
        ADD_MAP("net_skip_proxy", net_skip_proxy, boolean);
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
        ADD_MAP("vpn_impl", vpn_implementation, string);
        ADD_MAP("vpn_mtu", vpn_mtu, integer);
        ADD_MAP("vpn_ipv6", vpn_ipv6, boolean);
        ADD_MAP("vpn_strict_route", vpn_strict_route, boolean);
        ADD_MAP("sub_clear", sub_clear, boolean);
        ADD_MAP("net_insecure", net_insecure, boolean);
        ADD_MAP("sub_auto_update", sub_auto_update, integer);
        ADD_MAP("sub_send_hwid", sub_send_hwid, boolean);
        ADD_MAP("start_minimal", start_minimal, boolean);
        ADD_MAP("max_log_line", max_log_line, integer);
        ADD_MAP("splitter_state", splitter_state, string);
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
        ADD_MAP("disable_win_admin", disable_run_admin, boolean);
        ADD_MAP("enable_stats", enable_stats, boolean);
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

    #undef d_add
    #define d_add(X, Y, B) _put(ptr, X, &this->Y, itemType::B)

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
        ADD_MAP("use_dns_object", use_dns_object, boolean);
        ADD_MAP("dns_object", dns_object, string);
        ADD_MAP("dns_final_out", dns_final_out, string);
    STOP_MAP

    #undef d_add

    Shortcuts::Shortcuts() : JsonStore()
    {
    }
    
    DECL_MAP(Shortcuts)
        ADD_MAP("keyval", keyVal, stringList);
    STOP_MAP

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
