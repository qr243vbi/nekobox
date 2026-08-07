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
// Entity loaders (binary or legacy JSON, auto-detected)
// ============================================================================

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

/// Load a proxy entity from `profiles/<id>.cfg`.
pub fn load_proxy_entity(path: &Path) -> anyhow::Result<ProxyEntity> {
    if let Some(records) = read_binary_records(path)? {
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
        return Ok(e);
    }
    // Legacy JSON
    load_json(path)
}

/// Load the bean config (`beans/<id>.cfg`) as a JSON object
/// ready for `ProxyEntity::bean_cfg`.
pub fn load_bean_cfg(path: &Path) -> anyhow::Result<serde_json::Value> {
    if let Some(records) = read_binary_records(path)? {
        let map: serde_json::Map<String, serde_json::Value> = records
            .iter()
            .map(|(k, v)| (k.clone(), v.to_json()))
            .collect();
        return Ok(map.into());
    }
    load_json(path)
}

/// Load a group from `groups/<id>.cfg`.
pub fn load_group(path: &Path) -> anyhow::Result<Group> {
    if let Some(records) = read_binary_records(path)? {
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
        return Ok(g);
    }
    load_json(path)
}

/// Load subscription extras from `subscriptions/<id>.cfg`.
pub fn load_group_extra(path: &Path, id: i32) -> anyhow::Result<GroupExtra> {
    if let Some(records) = read_binary_records(path)? {
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
        return Ok(extra);
    }
    load_json(path)
}

/// Load the global DataStore from `nekobox.cfg`.
/// Unknown fields keep their defaults.
pub fn load_datastore(path: &Path) -> anyhow::Result<DataStore> {
    if let Some(records) = read_binary_records(path)? {
        let mut ds = DataStore::default();
        apply_datastore(&mut ds, &records);
        return Ok(ds);
    }
    load_json(path)
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
            "disable_traffic_stats" => {
                ds.disable_traffic_stats = b().unwrap_or(false)
            }
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
