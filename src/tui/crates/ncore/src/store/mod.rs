//! Store module — load `.cfg` files compatible with the C++ GUI.
//!
//! Files are stored in subdirectories under the base path:
//! - `profiles/<id>.cfg` — proxy entities
//! - `groups/<id>.cfg` — groups
//! - `beans/<id>.cfg` — per-profile protocol beans
//! - `route_profiles/<id>.cfg` — routing chains
//! - `subscriptions/<id>.cfg` — subscription extras (GroupExtra)
//! - `nekobox.cfg` — global DataStore
//!
//! Current GUI versions write a binary QDataStream format ("NekoBox"
//! magic + MD5-hashed field names, see [`binary`]); legacy files are
//! flat JSON. Both are detected and handled.

pub mod binary;

use std::path::{Path, PathBuf};

use crate::model::{DataStore, Group, GroupExtra, ProxyEntity, TunSplit};
use binary::BinValue;

/// The `NekoBox` store file (see `getJsonStoreFileName` in `Database.cpp`).
pub const DATASTORE_FILE: &str = "nekobox.cfg";

/// The `DefaultRoute` store file — holds DNS/domain-strategy/tun-split
/// settings and `current_route_id`.
pub const ROUTING_FILE: &str = "default_route_profile.cfg";

/// Base path for all config files.
/// Mirrors `GetBasePath()` in `Configs.cpp` which returns `QDir::currentPath()`.
pub fn get_base_path() -> PathBuf {
    std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."))
}

/// Get the directory for a store type.
pub fn get_store_path(base: &Path, store_type: &str) -> PathBuf {
    base.join(store_type)
}

/// Get the full path to a specific store file.
pub fn get_file_path(base: &Path, store_type: &str, id: i32) -> PathBuf {
    let dir = get_store_path(base, store_type);
    dir.join(format!("{}.cfg", id))
}

/// Load a JSON file into a deserializable value.
pub fn load_json<T: serde::de::DeserializeOwned>(path: &Path) -> anyhow::Result<T> {
    let data = std::fs::read_to_string(path)?;
    let value: T = serde_json::from_str(&data)?;
    Ok(value)
}

/// Save a serializable value to a JSON file.
pub fn save_json<T: serde::Serialize>(path: &Path, value: &T) -> anyhow::Result<()> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let json = serde_json::to_string_pretty(value)?;
    std::fs::write(path, json)?;
    Ok(())
}

/// Save a JSON string to a file.
pub fn save_json_bytes(path: &Path, json: &str) -> anyhow::Result<()> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    std::fs::write(path, json)?;
    Ok(())
}

/// List all files of a given store type in the base directory.
pub fn list_store_files(base: &Path, store_type: &str) -> anyhow::Result<Vec<(i32, PathBuf)>> {
    let dir = get_store_path(base, store_type);
    if !dir.exists() {
        return Ok(Vec::new());
    }
    let mut files = Vec::new();
    for entry in std::fs::read_dir(&dir)? {
        let entry = entry?;
        let path = entry.path();
        if path.extension().map(|e| e == "cfg") == Some(true) {
            if let Some(stem) = path.file_stem() {
                if let Ok(id) = stem.to_string_lossy().parse::<i32>() {
                    files.push((id, path));
                }
            }
        }
    }
    files.sort_by_key(|(id, _)| *id);
    Ok(files)
}

// ============================================================================
// Writers (JSON with C++ ADD_MAP key names — the GUI reads JSON .cfg
// files via its `FromJsonBytes` fallback when there is no "NekoBox" magic)
// ============================================================================

/// Next free id for a store directory.
pub fn next_store_id(base: &Path, store_type: &str) -> i32 {
    list_store_files(base, store_type)
        .map(|files| files.iter().map(|(id, _)| *id).max().unwrap_or(0) + 1)
        .unwrap_or(1)
}

/// Serialize a proxy entity (C++ key names).
pub fn save_proxy_entity(base: &Path, e: &ProxyEntity) -> anyhow::Result<()> {
    let value = serde_json::json!({
        "type": e.r#type,
        "id": e.id,
        "gid": e.gid,
        "yc": e.latency_int,
        "dl": e.dl_speed.clone().unwrap_or_default(),
        "ul": e.ul_speed.clone().unwrap_or_default(),
        "report": e.full_test_report.clone().unwrap_or_default(),
        "country": e.test_country.clone().unwrap_or_default(),
        "is_working": e.is_working,
        "last_auto_test_time": e.last_auto_test_time.unwrap_or(0),
        "name": e.name,
        "dtype": e.display_type.clone().unwrap_or_default(),
        "addr": e.server_address,
        "port": e.server_port,
        "traffic": { "dl": e.traffic_dl, "ul": e.traffic_ul },
    });
    save_json(&get_file_path(base, "profiles", e.id), &value)
}

/// Serialize a group (C++ key names).
pub fn save_group(base: &Path, g: &Group) -> anyhow::Result<()> {
    let value = serde_json::json!({
        "id": g.base.id,
        "front_proxy_id": g.front_proxy_id,
        "landing_proxy_id": g.landing_proxy_id,
        "archive": g.archive,
        "is_subscription": g.is_subscription,
        "name": g.name,
        "profiles": g.profiles,
    });
    save_json(&get_file_path(base, "groups", g.base.id), &value)
}

/// Serialize a bean config (`beans/<id>.cfg`). The bean JSON already uses
/// C++ ADD_MAP key names.
pub fn save_bean_cfg(base: &Path, id: i32, bean: &serde_json::Value) -> anyhow::Result<()> {
    save_json(&get_file_path(base, "beans", id), bean)
}

/// Serialize the DataStore (`nekobox.cfg`) with C++ key names.
///
/// The model covers only part of what the GUI stores (hotkeys, window state,
/// `data_store_type`, …), so the existing file is read first and the modelled
/// keys are overlaid onto it. Writing a fresh object instead would silently
/// drop every GUI-only setting the first time the TUI saved.
pub fn save_datastore(base: &Path, ds: &DataStore) -> anyhow::Result<()> {
    // Built imperatively: the json! macro hits its recursion limit with
    // this many fields.
    let mut m = read_existing_object(&base.join(DATASTORE_FILE));
    let mut put = |k: &str, v: serde_json::Value| {
        m.insert(k.to_string(), v);
    };
    put("core_use_uds", ds.core_use_uds.into());
    put("current_group", ds.current_group.into());
    put("inbound_address", ds.inbound_address.clone().into());
    put("inbound_socks_port", ds.inbound_socks_port.into());
    put(
        "inbound_username",
        ds.inbound_username.clone().unwrap_or_default().into(),
    );
    put(
        "inbound_password",
        ds.inbound_password.clone().unwrap_or_default().into(),
    );
    put("random_inbound_port", ds.random_inbound_port.into());
    put("custom_inbound", ds.custom_inbound.clone().into());
    put("log_level", ds.log_level.clone().into());
    put("mux_protocol", ds.mux_protocol.clone().into());
    put("mux_concurrency", ds.mux_concurrency.into());
    put("mux_padding", ds.mux_padding.into());
    put("mux_default_on", ds.mux_default_on.into());
    put("download_retries", ds.download_retries.into());
    put("download_timeout", ds.download_timeout.into());
    put("test_concurrent", ds.test_concurrent.into());
    put("ruleset_json_url", ds.ruleset_json_url.clone().into());
    put("ruleset_mirror", ds.ruleset_mirror.into());
    put("network_use_proxy", ds.network_use_proxy.into());
    put("net_insecure", ds.net_insecure.into());
    put("skip_cert", ds.skip_cert.into());
    put(
        "utlsFingerprint",
        ds.utls_fingerprint.clone().unwrap_or_default().into(),
    );
    put("active_routing", ds.active_routing.clone().into());
    put("disable_traffic_stats", ds.disable_traffic_stats.into());
    put("enable_stats", ds.connection_statistics.into());
    put("stats_tab", ds.stats_tab.into());
    put("disable_tray", ds.disable_tray.into());
    put("vpn_stack", ds.vpn_implementation.clone().into());
    put("vpn_mtu", ds.vpn_mtu.into());
    put("vpn_ipv6", ds.vpn_ipv6.into());
    put("vpn_strict_route", ds.vpn_strict_route.into());
    put("fakedns", ds.fake_dns.into());
    put("enable_tun_routing", ds.enable_tun_routing.into());
    put("tun_name", ds.tun_name.clone().into());
    put("tun_address", ds.tun_address.clone().into());
    put("tun_address_6", ds.tun_address_6.clone().into());
    put("remote_dns", ds.remote_dns.clone().into());
    put(
        "remote_dns_strategy",
        ds.remote_dns_strategy.clone().unwrap_or_default().into(),
    );
    put("direct_dns", ds.direct_dns.clone().into());
    put(
        "direct_dns_strategy",
        ds.direct_dns_strategy.clone().unwrap_or_default().into(),
    );
    put("use_dns_object", ds.use_dns_object.into());
    put(
        "dns_object",
        ds.dns_object.clone().unwrap_or_default().into(),
    );
    put("dns_final_out_direct", ds.dns_final_out_direct.into());
    put("domain_strategy", ds.domain_strategy.clone().into());
    put(
        "outbound_domain_strategy",
        ds.outbound_domain_strategy.clone().into(),
    );
    put("test_url", ds.test_latency_url.clone().into());
    put("urltest_timeout_ms", ds.url_test_timeout_ms.into());
    put("speedtest_timeout_ms", ds.speed_test_timeout_ms.into());
    put("speed_test_mode", ds.speed_test_mode.into());
    put("simple_dl_url", ds.simple_dl_url.clone().into());
    put("sub_auto_update", ds.sub_auto_update.into());
    put("sub_clear", ds.sub_clear.into());
    put("sub_send_hwid", ds.sub_send_hwid.into());
    put("sub_rm_invalid", ds.sub_rm_invalid.into());
    put("sub_url_test", ds.sub_url_test.into());
    put("sub_rm_duplicates", ds.sub_rm_duplicates.into());
    put("sub_rm_unavailable", ds.sub_rm_unavailable.into());
    put("adblock_enable", ds.adblock_enable.into());
    put("use_mozilla_certs", ds.use_mozilla_certs.into());
    put("system_dns_set", ds.system_dns_set.into());
    put("custom_route", ds.custom_route_global.clone().into());
    put("core_box_clash_api", ds.core_box_clash_api.into());
    put(
        "core_box_clash_listen_addr",
        ds.core_box_clash_listen_addr.clone().into(),
    );
    put("route_exclude_addrs", ds.route_exclude_addrs.clone().into());
    put("remember_id", ds.started_id.into());
    put("spmode2", ds.remember_spmode.clone().into());
    save_json(&base.join(DATASTORE_FILE), &serde_json::Value::Object(m))
}

/// Read an existing `.cfg` (binary or JSON) as a JSON object, so a partial
/// writer can overlay its keys without discarding the rest. Returns an empty
/// object when the file is missing or unreadable.
fn read_existing_object(path: &Path) -> serde_json::Map<String, serde_json::Value> {
    match read_records(path) {
        Ok(records) => binary::records_to_json(&records),
        Err(_) => serde_json::Map::new(),
    }
}

/// Delete a profile's files (`profiles/`, `beans/`).
pub fn delete_profile_files(base: &Path, id: i32) -> anyhow::Result<()> {
    for store in ["profiles", "beans"] {
        let path = get_file_path(base, store, id);
        if path.exists() {
            std::fs::remove_file(path)?;
        }
    }
    Ok(())
}

/// Read a `.cfg` file and parse it into records.
/// Returns `None` for legacy JSON files (caller falls back to JSON parsing).
fn read_binary_records(path: &Path) -> anyhow::Result<Option<Vec<(String, BinValue)>>> {
    let data = std::fs::read(path)?;
    if binary::is_binary(&data) {
        Ok(Some(binary::parse(&data)?))
    } else {
        Ok(None)
    }
}

/// Convert a JSON object (legacy/JSON-written `.cfg`) into records.
/// Keys are expected to be the C++ ADD_MAP names.
fn json_to_records(value: &serde_json::Value) -> Vec<(String, BinValue)> {
    let Some(obj) = value.as_object() else {
        return Vec::new();
    };
    obj.iter()
        .map(|(k, v)| (k.clone(), json_to_binvalue(v)))
        .collect()
}

fn json_to_binvalue(v: &serde_json::Value) -> BinValue {
    match v {
        serde_json::Value::Bool(b) => BinValue::Bool(*b),
        serde_json::Value::Number(n) => {
            if let Some(i) = n.as_i64() {
                if i >= i32::MIN as i64 && i <= i32::MAX as i64 {
                    BinValue::Int(i as i32)
                } else {
                    BinValue::Long(i)
                }
            } else {
                BinValue::Double(n.as_f64().unwrap_or(0.0))
            }
        }
        serde_json::Value::String(s) => BinValue::Str(s.clone()),
        serde_json::Value::Array(arr) => {
            if arr.iter().all(|v| v.is_string()) {
                BinValue::StrList(
                    arr.iter()
                        .filter_map(|v| v.as_str().map(str::to_string))
                        .collect(),
                )
            } else if arr.iter().all(|v| v.is_i64()) {
                BinValue::IntList(
                    arr.iter()
                        .filter_map(|v| v.as_i64().map(|i| i as i32))
                        .collect(),
                )
            } else {
                BinValue::StoreList(
                    arr.iter()
                        .map(json_to_records)
                        .collect(),
                )
            }
        }
        serde_json::Value::Object(_) => BinValue::Store(json_to_records(v)),
        serde_json::Value::Null => BinValue::Str(String::new()),
    }
}

/// Read records from a `.cfg` file regardless of the on-disk format
/// (binary QDataStream or JSON).
pub fn read_records(path: &Path) -> anyhow::Result<Vec<(String, BinValue)>> {
    if let Some(records) = read_binary_records(path)? {
        return Ok(records);
    }
    let data = std::fs::read_to_string(path)?;
    let value: serde_json::Value = serde_json::from_str(&data)?;
    Ok(json_to_records(&value))
}

/// Load a proxy entity from `profiles/<id>.cfg`.
pub fn load_proxy_entity(path: &Path) -> anyhow::Result<ProxyEntity> {
    let records = read_records(path)?;
    let mut e = ProxyEntity::new("");
    for (name, v) in &records {
        match name.as_str() {
            "type" => e.r#type = v.as_str().unwrap_or_default().into(),
            "id" => e.id = v.as_i64().unwrap_or(-1) as i32,
            "gid" => e.gid = v.as_i64().unwrap_or(0) as i32,
            "yc" => e.latency_int = v.as_i64().unwrap_or(0) as i32,
            "dl" => e.dl_speed = v.as_str().map(str::to_string),
            "ul" => e.ul_speed = v.as_str().map(str::to_string),
            "report" => e.full_test_report = v.as_str().map(str::to_string),
            "country" => e.test_country = v.as_str().map(str::to_string),
            "is_working" => e.is_working = v.as_bool().unwrap_or(false),
            "last_auto_test_time" => e.last_auto_test_time = v.as_i64(),
            "name" => e.name = v.as_str().unwrap_or_default().into(),
            "dtype" => e.display_type = v.as_str().map(str::to_string),
            "addr" => e.server_address = v.as_str().unwrap_or_default().into(),
            "port" => e.server_port = v.as_i64().unwrap_or(1080) as i32,
            "traffic" => {
                if let BinValue::Store(recs) = v {
                    for (n, tv) in recs {
                        match n.as_str() {
                            "dl" => e.traffic_dl = tv.as_i64().unwrap_or(0),
                            "ul" => e.traffic_ul = tv.as_i64().unwrap_or(0),
                            _ => {}
                        }
                    }
                }
            }
            _ => {}
        }
    }
    Ok(e)
}

/// Load the bean config (`beans/<id>.cfg`) as a JSON object
/// ready for `ProxyEntity::bean_cfg`.
pub fn load_bean_cfg(path: &Path) -> anyhow::Result<serde_json::Value> {
    let records = read_records(path)?;
    let map: serde_json::Map<String, serde_json::Value> = records
        .iter()
        .map(|(k, v)| (k.clone(), v.to_json()))
        .collect();
    Ok(map.into())
}

/// Load a group from `groups/<id>.cfg`.
pub fn load_group(path: &Path) -> anyhow::Result<Group> {
    let records = read_records(path)?;
    let mut g = Group::new();
    for (name, v) in &records {
        match name.as_str() {
            "id" => g.base.id = v.as_i64().unwrap_or(-1) as i32,
            "front_proxy_id" => g.front_proxy_id = v.as_i64().unwrap_or(-1) as i32,
            "landing_proxy_id" => g.landing_proxy_id = v.as_i64().unwrap_or(-1) as i32,
            "archive" => g.archive = v.as_bool().unwrap_or(false),
            "is_subscription" => g.is_subscription = v.as_bool().unwrap_or(false),
            "name" => g.name = v.as_str().unwrap_or_default().into(),
            "profiles" => {
                if let BinValue::IntList(ids) = v {
                    g.profiles = ids.clone();
                }
            }
            _ => {}
        }
    }
    Ok(g)
}

/// Load subscription extras from `subscriptions/<id>.cfg`.
pub fn load_group_extra(path: &Path, id: i32) -> anyhow::Result<GroupExtra> {
    let records = read_records(path)?;
    let mut extra = GroupExtra {
        id,
        ..Default::default()
    };
    for (name, v) in &records {
        match name.as_str() {
            "enable_custom_headers" => extra.enable_custom_headers = v.as_bool().unwrap_or(false),
            "enable_custom_payload" => extra.enable_custom_payload = v.as_bool().unwrap_or(false),
            "enable_hwid" => extra.enable_hwid = v.as_bool().unwrap_or(false),
            "custom_hwid" => extra.custom_hwid = v.as_str().map(str::to_string),
            "url" => extra.url = v.as_str().map(str::to_string),
            "info" => extra.info = v.as_str().map(str::to_string),
            "sub_last_update" => extra.sub_last_update = v.as_i64(),
            "skip_auto_update" => extra.skip_auto_update = v.as_bool().unwrap_or(false),
            "text_payload" => extra.text_payload = v.as_str().map(str::to_string),
            "javascript_payload" => {
                extra.javascript_payload = v.as_str().map(str::to_string)
            }
            "custom_headers" => {
                if let BinValue::StrMap(entries) = v {
                    extra.custom_headers = Some(
                        entries
                            .iter()
                            .map(|(k, v)| (k.clone(), v.as_str().unwrap_or_default().into()))
                            .collect(),
                    );
                }
            }
            _ => {}
        }
    }
    Ok(extra)
}

/// Load a routing chain from `route_profiles/<id>.cfg`.
pub fn load_route_chain(path: &Path) -> anyhow::Result<crate::model::RoutingChain> {
    use crate::model::RoutingChain;
    let records = read_records(path)?;
    let mut chain = RoutingChain::default();
    for (name, v) in &records {
        match name.as_str() {
            "id" => chain.base.id = v.as_i64().unwrap_or(-1) as i32,
            "name" => chain.chain_name = v.as_str().unwrap_or_default().into(),
            "update_url" => chain.update_url = v.as_str().map(str::to_string),
            "skip_update" => chain.skip_update = v.as_bool().unwrap_or(false),
            "default_outbound" => {
                chain.default_outbound_id = v.as_i64().unwrap_or(-2) as i32
            }
            "rules" => {
                if let BinValue::StoreList(items) = v {
                    chain.rules = items
                        .iter()
                        .map(|recs| records_to_rule(recs))
                        .collect();
                }
            }
            _ => {}
        }
    }
    Ok(chain)
}

fn records_to_rule(records: &[(String, BinValue)]) -> crate::model::RouteRule {
    let mut r = crate::model::RouteRule::default();
    let str_list = |v: &BinValue| -> Vec<String> {
        match v {
            BinValue::StrList(l) => l.clone(),
            _ => Vec::new(),
        }
    };
    for (name, v) in records {
        match name.as_str() {
            "name" => r.name = v.as_str().unwrap_or_default().into(),
            "type" => r.r#type = v.as_i64().unwrap_or(0) as i32,
            "simple_action" => r.simple_action = v.as_i64().unwrap_or(0) as i32,
            "ip_version" => r.ip_version = v.as_str().map(str::to_string),
            "network" => r.network = v.as_str().map(str::to_string),
            "protocol" => r.protocol = v.as_str().map(str::to_string),
            "inbound" => r.inbound = str_list(v),
            "domain" => r.domain = str_list(v),
            "domain_suffix" => r.domain_suffix = str_list(v),
            "domain_keyword" => r.domain_keyword = str_list(v),
            "domain_regex" => r.domain_regex = str_list(v),
            "source_ip_cidr" => r.source_ip_cidr = str_list(v),
            "source_ip_is_private" => r.source_ip_is_private = v.as_bool().unwrap_or(false),
            "ip_cidr" => r.ip_cidr = str_list(v),
            "ip_is_private" => r.ip_is_private = v.as_bool().unwrap_or(false),
            "source_port" => r.source_port = str_list(v),
            "source_port_range" => r.source_port_range = str_list(v),
            "port" => r.port = str_list(v),
            "port_range" => r.port_range = str_list(v),
            "process_name" => r.process_name = str_list(v),
            "process_path" => r.process_path = str_list(v),
            "process_path_regex" => r.process_path_regex = str_list(v),
            "rule_set" => r.rule_set = str_list(v),
            "invert" => r.invert = v.as_bool().unwrap_or(false),
            "outboundID" => r.outbound_id = v.as_i64().unwrap_or(-2) as i32,
            "actionType" => r.action = v.as_str().unwrap_or("route").into(),
            "rejectMethod" => r.reject_method = v.as_str().map(str::to_string),
            "noDrop" => r.no_drop = v.as_bool().unwrap_or(false),
            "override_address" => r.override_address = v.as_str().map(str::to_string),
            "override_port" => r.override_port = v.as_i64().map(|x| x.to_string()),
            "sniffers" => r.sniffers = str_list(v),
            "sniffOverrideDest" => r.sniff_override_dest = v.as_bool().unwrap_or(false),
            "strategy" => r.strategy = v.as_str().map(str::to_string),
            _ => {}
        }
    }
    r
}

/// Load the global DataStore from `nekobox.cfg`.
/// Unknown fields keep their defaults.
pub fn load_datastore(path: &Path) -> anyhow::Result<DataStore> {
    let mut ds = DataStore::default();
    let records = read_records(path)?;
    apply_datastore(&mut ds, &records);
    Ok(ds)
}

/// Overlay `default_route_profile.cfg` onto an already-loaded [`DataStore`].
///
/// The GUI splits its settings across two files: `nekobox.cfg` (the `NekoBox`
/// store) and `default_route_profile.cfg` (the `Routing` store). DNS, domain
/// strategy, ruleset, tun-split and — importantly — `current_route_id` live in
/// the latter, so a DataStore built from `nekobox.cfg` alone silently falls
/// back to defaults for all of them.
///
/// Missing file is not an error: the GUI writes it lazily.
pub fn load_routing_into(base: &Path, ds: &mut DataStore) -> anyhow::Result<()> {
    let path = base.join(ROUTING_FILE);
    if !path.exists() {
        return Ok(());
    }
    let records = read_records(&path)?;
    apply_datastore(ds, &records);
    Ok(())
}

/// Load the full GUI settings: `nekobox.cfg` overlaid with
/// `default_route_profile.cfg`. This is what callers should use.
pub fn load_settings(base: &Path) -> DataStore {
    let mut ds = load_datastore(&base.join(DATASTORE_FILE)).unwrap_or_default();
    let _ = load_routing_into(base, &mut ds);
    ds
}

/// Write back the `Routing` half of the settings (`default_route_profile.cfg`).
///
/// Only the keys the C++ `Routing::_map()` declares are written, so the file
/// stays loadable by the GUI.
pub fn save_routing(base: &Path, ds: &DataStore) -> anyhow::Result<()> {
    let path = base.join(ROUTING_FILE);
    let mut m = read_existing_object(&path);
    let known = serde_json::json!({
        "current_route_id": ds.current_route_id,
        "imported_group": ds.imported_group,
        "remote_dns": ds.remote_dns,
        "remote_dns_strategy": ds.remote_dns_strategy.clone().unwrap_or_default(),
        "direct_dns": ds.direct_dns,
        "direct_dns_strategy": ds.direct_dns_strategy.clone().unwrap_or_default(),
        "domain_strategy": ds.domain_strategy,
        "outbound_domain_strategy": ds.outbound_domain_strategy,
        "sniffing_mode": ds.sniffing_mode,
        "ruleset_mirror": ds.ruleset_mirror,
        "ruleset_json_url": ds.ruleset_json_url,
        "use_dns_object": ds.use_dns_object,
        "dns_object": ds.dns_object.clone().unwrap_or_default(),
        "dns_final_out_direct": ds.dns_final_out_direct,
        "tun_split": {
            "proxy": ds.tun_split.proxy,
            "direct": ds.tun_split.direct,
            "block": ds.tun_split.block,
        },
    });
    if let serde_json::Value::Object(known) = known {
        m.extend(known);
    }
    save_json(&path, &serde_json::Value::Object(m))
}

/// Save both halves of the settings.
pub fn save_settings(base: &Path, ds: &DataStore) -> anyhow::Result<()> {
    save_datastore(base, ds)?;
    save_routing(base, ds)
}

/// Serialize a routing chain to `route_profiles/<id>.cfg` (C++ key names).
pub fn save_route_chain(base: &Path, chain: &crate::model::RoutingChain) -> anyhow::Result<()> {
    let value = serde_json::json!({
        "id": chain.base.id,
        "name": chain.chain_name,
        "update_url": chain.update_url.clone().unwrap_or_default(),
        "skip_update": chain.skip_update,
        "default_outbound": chain.default_outbound_id,
        "rules": chain.rules.iter().map(rule_to_store_json).collect::<Vec<_>>(),
    });
    save_json(&get_file_path(base, "route_profiles", chain.base.id), &value)
}

/// A rule as stored on disk (C++ `RouteRule::_map()` key names — note these
/// differ from the sing-box JSON emitted by the config builder).
fn rule_to_store_json(r: &crate::model::RouteRule) -> serde_json::Value {
    serde_json::json!({
        "name": r.name,
        "type": r.r#type,
        "simple_action": r.simple_action,
        "ip_version": r.ip_version.clone().unwrap_or_default(),
        "network": r.network.clone().unwrap_or_default(),
        "protocol": r.protocol.clone().unwrap_or_default(),
        "inbound": r.inbound,
        "domain": r.domain,
        "domain_suffix": r.domain_suffix,
        "domain_keyword": r.domain_keyword,
        "domain_regex": r.domain_regex,
        "source_ip_cidr": r.source_ip_cidr,
        "source_ip_is_private": r.source_ip_is_private,
        "ip_cidr": r.ip_cidr,
        "ip_is_private": r.ip_is_private,
        "source_port": r.source_port,
        "source_port_range": r.source_port_range,
        "port": r.port,
        "port_range": r.port_range,
        "process_name": r.process_name,
        "process_path": r.process_path,
        "process_path_regex": r.process_path_regex,
        "rule_set": r.rule_set,
        "invert": r.invert,
        "outboundID": r.outbound_id,
        "actionType": r.action,
        "rejectMethod": r.reject_method.clone().unwrap_or_default(),
        "noDrop": r.no_drop,
        "override_address": r.override_address.clone().unwrap_or_default(),
        "override_port": r.override_port.as_deref().and_then(|s| s.parse::<i32>().ok()).unwrap_or(0),
        "sniffers": r.sniffers,
        "sniffOverrideDest": r.sniff_override_dest,
        "strategy": r.strategy.clone().unwrap_or_default(),
    })
}

/// Serialize a group's subscription extras (`subscriptions/<id>.cfg`).
pub fn save_group_extra(base: &Path, extra: &crate::model::GroupExtra) -> anyhow::Result<()> {
    let value = serde_json::json!({
        "id": extra.id,
        "enable_custom_headers": extra.enable_custom_headers,
        "enable_custom_payload": extra.enable_custom_payload,
        "enable_hwid": extra.enable_hwid,
        "custom_hwid": extra.custom_hwid.clone().unwrap_or_default(),
        "text_payload": extra.text_payload.clone().unwrap_or_default(),
        "javascript_payload": extra.javascript_payload.clone().unwrap_or_default(),
        "url": extra.url.clone().unwrap_or_default(),
        "info": extra.info.clone().unwrap_or_default(),
        "sub_last_update": extra.sub_last_update.unwrap_or(0),
        "skip_auto_update": extra.skip_auto_update,
    });
    save_json(&get_file_path(base, "subscriptions", extra.id), &value)
}

/// Delete a group's files (`groups/`, `subscriptions/`).
pub fn delete_group_files(base: &Path, id: i32) -> anyhow::Result<()> {
    for store in ["groups", "subscriptions"] {
        let path = get_file_path(base, store, id);
        if path.exists() {
            std::fs::remove_file(path)?;
        }
    }
    Ok(())
}

/// Map on-disk DataStore record names onto the model.
fn apply_datastore(ds: &mut DataStore, records: &[(String, BinValue)]) {
    for (name, v) in records {
        let s = || v.as_str().map(str::to_string);
        let i = || v.as_i64().map(|x| x as i32);
        let b = || v.as_bool();
        match name.as_str() {
            "core_use_uds" => ds.core_use_uds = b().unwrap_or(ds.core_use_uds),
            "current_group" => ds.current_group = i().unwrap_or(ds.current_group),
            "inbound_address" => ds.inbound_address = s().unwrap_or(ds.inbound_address.clone()),
            "inbound_socks_port" => {
                ds.inbound_socks_port = i().unwrap_or(ds.inbound_socks_port)
            }
            "inbound_username" => ds.inbound_username = s(),
            "inbound_password" => ds.inbound_password = s(),
            "random_inbound_port" => ds.random_inbound_port = b().unwrap_or(false),
            "custom_inbound" => ds.custom_inbound = s().unwrap_or(ds.custom_inbound.clone()),
            "log_level" => ds.log_level = s().unwrap_or(ds.log_level.clone()),
            "mux_protocol" => ds.mux_protocol = s().unwrap_or(ds.mux_protocol.clone()),
            "mux_concurrency" => ds.mux_concurrency = i().unwrap_or(ds.mux_concurrency),
            "mux_padding" => ds.mux_padding = b().unwrap_or(false),
            "mux_default_on" => ds.mux_default_on = b().unwrap_or(false),
            "download_retries" => ds.download_retries = i().unwrap_or(ds.download_retries),
            "download_timeout" => ds.download_timeout = i().unwrap_or(ds.download_timeout),
            "test_concurrent" => ds.test_concurrent = i().unwrap_or(ds.test_concurrent),
            "ruleset_json_url" => {
                ds.ruleset_json_url = s().unwrap_or(ds.ruleset_json_url.clone())
            }
            "ruleset_mirror" => ds.ruleset_mirror = i().unwrap_or(ds.ruleset_mirror),
            "network_use_proxy" => ds.network_use_proxy = b().unwrap_or(ds.network_use_proxy),
            "net_insecure" => ds.net_insecure = b().unwrap_or(false),
            "skip_cert" => ds.skip_cert = b().unwrap_or(false),
            "utlsFingerprint" => ds.utls_fingerprint = s(),
            "active_routing" => ds.active_routing = s().unwrap_or(ds.active_routing.clone()),
            "current_route_id" => ds.current_route_id = i().unwrap_or(ds.current_route_id),
            "imported_group" => ds.imported_group = i().unwrap_or(ds.imported_group),
            "sniffing_mode" => ds.sniffing_mode = i().unwrap_or(ds.sniffing_mode),
            "remember_id" => ds.started_id = i().unwrap_or(ds.started_id),
            "spmode2" => {
                if let BinValue::StrList(list) = v {
                    ds.remember_spmode = list.clone();
                }
            }
            "disable_traffic_stats" => {
                ds.disable_traffic_stats = b().unwrap_or(false)
            }
            // The C++ name for `connection_statistics`.
            "enable_stats" => {
                ds.connection_statistics = b().unwrap_or(ds.connection_statistics)
            }
            "stats_tab" => ds.stats_tab = i().unwrap_or(ds.stats_tab),
            "disable_tray" => ds.disable_tray = b().unwrap_or(false),
            "vpn_stack" => {
                ds.vpn_implementation = match v {
                    BinValue::Enum(n) => {
                        binary::resolve_enum("vpn_stack", *n).unwrap_or("").into()
                    }
                    _ => s().unwrap_or(ds.vpn_implementation.clone()),
                }
            }
            "vpn_mtu" => ds.vpn_mtu = i().unwrap_or(ds.vpn_mtu),
            "vpn_ipv6" => ds.vpn_ipv6 = b().unwrap_or(false),
            "vpn_strict_route" => ds.vpn_strict_route = b().unwrap_or(ds.vpn_strict_route),
            "fakedns" => ds.fake_dns = b().unwrap_or(false),
            "enable_tun_routing" => ds.enable_tun_routing = b().unwrap_or(false),
            "tun_name" => ds.tun_name = s().unwrap_or(ds.tun_name.clone()),
            "tun_address" => ds.tun_address = s().unwrap_or(ds.tun_address.clone()),
            "tun_address_6" => ds.tun_address_6 = s().unwrap_or(ds.tun_address_6.clone()),
            "remote_dns" => ds.remote_dns = s().unwrap_or(ds.remote_dns.clone()),
            "remote_dns_strategy" => ds.remote_dns_strategy = s(),
            "direct_dns" => ds.direct_dns = s().unwrap_or(ds.direct_dns.clone()),
            "direct_dns_strategy" => ds.direct_dns_strategy = s(),
            "use_dns_object" => ds.use_dns_object = b().unwrap_or(false),
            "dns_object" => ds.dns_object = s(),
            "dns_final_out_direct" => ds.dns_final_out_direct = b().unwrap_or(false),
            "domain_strategy" => {
                ds.domain_strategy = s().unwrap_or(ds.domain_strategy.clone())
            }
            "outbound_domain_strategy" => {
                ds.outbound_domain_strategy =
                    s().unwrap_or(ds.outbound_domain_strategy.clone())
            }
            "test_url" => ds.test_latency_url = s().unwrap_or(ds.test_latency_url.clone()),
            "urltest_timeout_ms" => {
                ds.url_test_timeout_ms = i().unwrap_or(ds.url_test_timeout_ms)
            }
            "speedtest_timeout_ms" => {
                ds.speed_test_timeout_ms = i().unwrap_or(ds.speed_test_timeout_ms)
            }
            "speed_test_mode" => ds.speed_test_mode = i().unwrap_or(ds.speed_test_mode),
            "simple_dl_url" => ds.simple_dl_url = s().unwrap_or(ds.simple_dl_url.clone()),
            "sub_auto_update" => ds.sub_auto_update = i().unwrap_or(ds.sub_auto_update),
            "sub_clear" => ds.sub_clear = b().unwrap_or(false),
            "sub_send_hwid" => ds.sub_send_hwid = b().unwrap_or(false),
            "sub_rm_invalid" => ds.sub_rm_invalid = b().unwrap_or(false),
            "sub_url_test" => ds.sub_url_test = b().unwrap_or(false),
            "sub_rm_duplicates" => ds.sub_rm_duplicates = b().unwrap_or(false),
            "sub_rm_unavailable" => ds.sub_rm_unavailable = b().unwrap_or(false),
            "adblock_enable" => ds.adblock_enable = b().unwrap_or(false),
            "use_mozilla_certs" => ds.use_mozilla_certs = b().unwrap_or(false),
            "system_dns_set" => ds.system_dns_set = b().unwrap_or(false),
            "enable_ntp" => ds.enable_ntp = b().unwrap_or(false),
            "ntp_server_address" => ds.ntp_server_address = s(),
            "ntp_server_port" => ds.ntp_server_port = i().unwrap_or(0),
            "ntp_interval" => ds.ntp_interval = s(),
            "enable_dns_server" => ds.enable_dns_server = b().unwrap_or(false),
            "dns_server_listen_lan" => ds.dns_server_listen_lan = b().unwrap_or(false),
            "dns_server_listen_port" => {
                ds.dns_server_listen_port = i().unwrap_or(ds.dns_server_listen_port)
            }
            "dns_v4_resp" => ds.dns_v4_resp = s().unwrap_or(ds.dns_v4_resp.clone()),
            "dns_v6_resp" => ds.dns_v6_resp = s().unwrap_or(ds.dns_v6_resp.clone()),
            "dns_server_rules" => {
                if let BinValue::StrList(list) = v {
                    ds.dns_server_rules = list.clone();
                }
            }
            "enable_redirect" => ds.enable_redirect = b().unwrap_or(false),
            "redirect_listen_address" => {
                ds.redirect_listen_address = s().unwrap_or(ds.redirect_listen_address.clone())
            }
            "redirect_listen_port" => {
                ds.redirect_listen_port = i().unwrap_or(ds.redirect_listen_port)
            }
            "route_exclude_addrs" => {
                if let BinValue::StrList(list) = v {
                    ds.route_exclude_addrs = list.clone();
                }
            }
            "tun_split" => {
                if let Ok(ts) = serde_json::from_value::<TunSplit>(v.to_json()) {
                    ds.tun_split = ts;
                }
            }
            "custom_route" => {
                ds.custom_route_global = s().unwrap_or(ds.custom_route_global.clone())
            }
            "core_box_clash_api" => ds.core_box_clash_api = i().unwrap_or(ds.core_box_clash_api),
            "core_box_clash_listen_addr" => {
                ds.core_box_clash_listen_addr =
                    s().unwrap_or(ds.core_box_clash_listen_addr.clone())
            }
            "core_box_clash_api_secret" => ds.core_box_clash_api_secret = s(),
            "core_box_underlying_dns" => ds.core_box_underlying_dns = s(),
            "auto_test_enable" => ds.auto_test_enable = b().unwrap_or(false),
            "auto_test_interval_seconds" => {
                ds.auto_test_interval_seconds = i().unwrap_or(0)
            }
            "auto_test_proxy_count" => ds.auto_test_proxy_count = i().unwrap_or(0),
            "auto_test_working_pool_size" => {
                ds.auto_test_working_pool_size = i().unwrap_or(0)
            }
            "auto_test_latency_threshold_ms" => {
                ds.auto_test_latency_threshold_ms = i().unwrap_or(0)
            }
            "auto_test_failure_retry_count" => {
                ds.auto_test_failure_retry_count = i().unwrap_or(0)
            }
            "auto_test_target_url" => ds.auto_test_target_url = s(),
            "auto_test_tun_failover" => {
                ds.auto_test_tun_failover = b().unwrap_or(ds.auto_test_tun_failover)
            }
            _ => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_store_path() {
        let base = PathBuf::from("/tmp/test");
        let profiles = get_store_path(&base, "profiles");
        assert_eq!(profiles, PathBuf::from("/tmp/test/profiles"));
    }

    #[test]
    fn test_get_file_path() {
        let base = PathBuf::from("/tmp/test");
        let file = get_file_path(&base, "profiles", 42);
        assert_eq!(file, PathBuf::from("/tmp/test/profiles/42.cfg"));
    }
}
