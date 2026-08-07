//! ConfigBuilder — model → sing-box JSON config.
//!
//! Port of `src/nekobox/configs/ConfigBuilder.hpp` and
//! `src/gharqad/configs/proxy/*Bean.cpp` (`BuildCoreObjSingBox`).
//!
//! The ConfigBuilder takes a `ProxyEntity` (with its bean config), a
//! `DataStore`, and optionally a `RoutingChain`, and produces a
//! complete sing-box JSON config suitable for passing to the core via
//! `Start(LoadConfigReq)`.

use crate::model::{DataStore, ProxyEntity};
use serde_json::{json, Value};

/// Build the sing-box outbound object for a proxy entity.
///
/// Port of `BuildCoreObjSingBox` from the per-protocol beans
/// (`ShadowSocksBean`, `VMessBean`, `TrojanVLESSBean`, ...).
/// `add_default_fields` always contributes `type`/`server`/`server_port`.
pub fn build_outbound(proxy: &ProxyEntity) -> Value {
    let bean = proxy.bean_cfg.clone().unwrap_or_else(|| json!({}));

    let mut out = json!({
        "type": sing_box_type(&proxy.r#type),
        "server": proxy.server_address,
        "server_port": proxy.server_port,
    });

    match proxy.r#type.as_str() {
        "shadowsocks" => {
            // On-disk bean: {"method", "pass", "plugin", "plugin_opts", "uot", "network"}
            set(&mut out, "method", str_or(&bean, &["method"], "aes-128-gcm"));
            set(&mut out, "password", str_or(&bean, &["pass", "password"], ""));
            set_non_empty(&mut out, "plugin", get_str(&bean, &["plugin"]));
            set_non_empty(&mut out, "plugin_opts", get_str(&bean, &["plugin_opts"]));
            add_network(&mut out, &bean);
            add_udp_over_tcp(&mut out, &bean);
        }
        "vmess" => {
            // On-disk bean: {"id", "aid", "sec", "network", "stream"}
            set(&mut out, "uuid", str_or(&bean, &["id", "uuid"], ""));
            out["alter_id"] = json!(bean.get("aid").and_then(Value::as_i64).unwrap_or(0));
            set(
                &mut out,
                "security",
                str_or(&bean, &["sec", "security"], "auto"),
            );
            add_network(&mut out, &bean);
            add_stream_settings(&mut out, &bean);
        }
        "vless" | "trojan" => {
            // On-disk bean: {"network", "pass", "flow", "stream", "enc"}
            if proxy.r#type == "vless" {
                set(&mut out, "uuid", str_or(&bean, &["pass", "id", "uuid"], ""));
                let flow = get_str(&bean, &["flow"]);
                match flow {
                    Some("none") | None => {}
                    Some(f) => set(&mut out, "flow", f.trim_end_matches("-udp443")),
                }
                set_non_empty(&mut out, "encryption", get_str(&bean, &["enc"]));
            } else {
                set(&mut out, "password", str_or(&bean, &["pass", "password"], ""));
            }
            add_network(&mut out, &bean);
            add_stream_settings(&mut out, &bean);
        }
        "socks" => {
            set(&mut out, "version", "5");
            set_non_empty(&mut out, "username", get_str(&bean, &["username"]));
            set_non_empty(
                &mut out,
                "password",
                get_str(&bean, &["pass", "password"]),
            );
            add_network(&mut out, &bean);
            add_udp_over_tcp(&mut out, &bean);
        }
        "http" => {
            set_non_empty(&mut out, "username", get_str(&bean, &["username"]));
            set_non_empty(
                &mut out,
                "password",
                get_str(&bean, &["pass", "password"]),
            );
            add_stream_settings(&mut out, &bean);
        }
        "direct" | "block" | "dns" => {
            // Type-only outbounds; nothing else needed.
            return json!({ "type": sing_box_type(&proxy.r#type) });
        }
        "hysteria" | "hysteria2" | "tuic" => {
            // Port of QUICBean::BuildCoreObjSingBox.
            // Disk keys: forceExternal, allowInsecure, sni, alpn, caText,
            // disableSni, authPayload(Type), obfsPassword, uploadMbps,
            // downloadMbps, streamReceiveWindow, connectionReceiveWindow,
            // disableMtuDiscovery, server_ports, hop_interval, password,
            // uuid, congestionControl, udpRelayMode, zeroRttHandshake,
            // heartbeat, uos.
            let mut tls = json!({
                "enabled": true,
                "server_name": str_or(&bean, &["sni"], ""),
                "insecure": get_bool(&bean, &["allowInsecure"], false),
                "disable_sni": get_bool(&bean, &["disableSni"], false),
            });
            set_non_empty(&mut tls, "certificate", get_str(&bean, &["caText"]));
            if proxy.r#type == "hysteria2" {
                tls["alpn"] = json!("h3");
            } else if let Some(alpn) = get_str(&bean, &["alpn"]) {
                if !alpn.is_empty() {
                    tls["alpn"] = json!(alpn.split(',').collect::<Vec<_>>());
                }
            }
            out["tls"] = tls;
            add_network(&mut out, &bean);

            let server_ports = get_str_list(&bean, &["server_ports"]);
            if !server_ports.is_empty() {
                // "443" → "443:443" (C++ behaviour)
                let ports: Vec<String> = server_ports
                    .iter()
                    .map(|p| {
                        if p.contains(':') {
                            p.clone()
                        } else {
                            format!("{p}:{p}")
                        }
                    })
                    .collect();
                out.as_object_mut().unwrap().remove("server_port");
                out["server_ports"] = json!(ports);
                set_non_empty(&mut out, "hop_interval", get_str(&bean, &["hop_interval"]));
            }

            match proxy.r#type.as_str() {
                "hysteria" => {
                    set(&mut out, "obfs", str_or(&bean, &["obfsPassword"], ""));
                    out["disable_mtu_discovery"] =
                        json!(get_bool(&bean, &["disableMtuDiscovery"], false));
                    out["recv_window"] =
                        json!(get_i64(&bean, &["streamReceiveWindow"], 0));
                    out["recv_window_conn"] =
                        json!(get_i64(&bean, &["connectionReceiveWindow"], 0));
                    out["up_mbps"] = json!(get_i64(&bean, &["uploadMbps"], 100));
                    out["down_mbps"] = json!(get_i64(&bean, &["downloadMbps"], 100));
                    match get_i64(&bean, &["authPayloadType"], 0) {
                        2 => set(&mut out, "auth", str_or(&bean, &["authPayload"], "")),
                        1 => set(&mut out, "auth_str", str_or(&bean, &["authPayload"], "")),
                        _ => {}
                    }
                }
                "hysteria2" => {
                    set(&mut out, "password", str_or(&bean, &["password"], ""));
                    out["up_mbps"] = json!(get_i64(&bean, &["uploadMbps"], 100));
                    out["down_mbps"] = json!(get_i64(&bean, &["downloadMbps"], 100));
                    if let Some(obfs) = get_str(&bean, &["obfsPassword"]) {
                        if !obfs.is_empty() {
                            out["obfs"] = json!({
                                "type": "salamander",
                                "password": obfs,
                            });
                        }
                    }
                }
                _ => {
                    // tuic
                    set(&mut out, "uuid", str_or(&bean, &["uuid"], ""));
                    set(&mut out, "password", str_or(&bean, &["password"], ""));
                    set(
                        &mut out,
                        "congestion_control",
                        str_or(&bean, &["congestionControl"], "bbr"),
                    );
                    if get_bool(&bean, &["uos"], false) {
                        out["udp_over_stream"] = json!(true);
                    } else {
                        set(
                            &mut out,
                            "udp_relay_mode",
                            str_or(&bean, &["udpRelayMode"], "native"),
                        );
                    }
                    out["zero_rtt_handshake"] =
                        json!(get_bool(&bean, &["zeroRttHandshake"], false));
                    set_non_empty(&mut out, "heartbeat", get_str(&bean, &["heartbeat"]));
                }
            }
        }
        "anytls" => {
            // Port of AnyTLSBean::BuildCoreObjSingBox.
            set(&mut out, "password", str_or(&bean, &["password"], ""));
            set(
                &mut out,
                "idle_session_check_interval",
                str_or(&bean, &["session_idle_check_interval"], "30s"),
            );
            set(
                &mut out,
                "idle_session_timeout",
                str_or(&bean, &["session_idle_timeout"], "30s"),
            );
            out["min_idle_session"] = json!(get_i64(&bean, &["min_idle_session"], 0));
            add_stream_settings(&mut out, &bean);
        }
        _ => {
            // Best effort for the remaining protocols (wireguard, custom,
            // ...): merge the bean over the defaults, dropping bean-internal
            // bookkeeping keys.
            const INTERNAL: &[&str] = &[
                "_v", "c_cfg", "c_out", "mux", "enable_brutal", "brutal_speed",
                "stream", "network",
            ];
            if let Some(obj) = bean.as_object() {
                for (k, v) in obj {
                    if !INTERNAL.contains(&k.as_str()) {
                        out[k.clone()] = v.clone();
                    }
                }
            }
            add_stream_settings(&mut out, &bean);
        }
    }

    out
}

/// Map the nekobox protocol type to the sing-box outbound type.
fn sing_box_type(t: &str) -> String {
    match t {
        "hysteria" => "hysteria".into(),
        other => other.into(),
    }
}

// --- bean field helpers ---

fn get_str<'a>(bean: &'a Value, keys: &[&str]) -> Option<&'a str> {
    keys.iter().find_map(|k| bean.get(*k).and_then(Value::as_str))
}

fn str_or<'a>(bean: &'a Value, keys: &[&str], default: &'a str) -> &'a str {
    get_str(bean, keys).unwrap_or(default)
}

fn get_bool(bean: &Value, keys: &[&str], default: bool) -> bool {
    keys.iter()
        .find_map(|k| bean.get(*k).and_then(Value::as_bool))
        .unwrap_or(default)
}

fn get_i64(bean: &Value, keys: &[&str], default: i64) -> i64 {
    keys.iter()
        .find_map(|k| bean.get(*k).and_then(Value::as_i64))
        .unwrap_or(default)
}

fn get_str_list(bean: &Value, keys: &[&str]) -> Vec<String> {
    keys.iter()
        .find_map(|k| {
            bean.get(*k).and_then(Value::as_array).map(|arr| {
                arr.iter()
                    .filter_map(|v| v.as_str().map(str::to_string))
                    .collect()
            })
        })
        .unwrap_or_default()
}

fn set(out: &mut Value, key: &str, value: impl Into<Value>) {
    out[key] = value.into();
}

fn set_non_empty(out: &mut Value, key: &str, value: Option<&str>) {
    if let Some(v) = value {
        if !v.is_empty() {
            out[key] = json!(v);
        }
    }
}

/// `add_network`: non-tcp network goes to the outbound top level (hysteria etc.)
fn add_network(out: &mut Value, bean: &Value) {
    if let Some(net) = get_str(bean, &["network"]) {
        if !net.is_empty() && net != "tcp" {
            out["network"] = json!(net);
        }
    }
}

/// `add_udp_over_tcp`: uot=0 → false, otherwise an object.
fn add_udp_over_tcp(out: &mut Value, bean: &Value) {
    let uot = bean.get("uot").and_then(Value::as_i64).unwrap_or(0);
    out["udp_over_tcp"] = if uot <= 0 {
        json!(false)
    } else {
        json!({ "enabled": true, "version": uot })
    };
}

/// Port of `V2rayStreamSettings::BuildStreamSettingsSingBox` (MVP subset):
/// ws/grpc/http/httpupgrade transports and TLS (sni/alpn/insecure).
///
/// Tolerates both key spellings: on-disk GUI beans use `net`/`sec`,
/// our share-link parser writes `network`/`tls`.
fn add_stream_settings(out: &mut Value, bean: &Value) {
    let empty = json!({});
    let stream = bean.get("stream").unwrap_or(&empty);

    let network = get_str(stream, &["net", "network"]).unwrap_or("tcp");
    let path = get_str(stream, &["path"]).unwrap_or("");
    let host = get_str(stream, &["host"]).unwrap_or("");

    if !network.is_empty() && network != "tcp" {
        let mut transport = json!({ "type": network });
        match network {
            "ws" => {
                set_non_empty(&mut transport, "path", Some(path));
                if !host.is_empty() {
                    transport["headers"] = json!({ "Host": host });
                }
            }
            "grpc" => {
                set_non_empty(&mut transport, "service_name", Some(path));
            }
            "http" | "httpupgrade" => {
                set_non_empty(&mut transport, "path", Some(path));
                set_non_empty(&mut transport, "host", Some(host));
            }
            _ => {}
        }
        out["transport"] = transport;
    }

    let security = get_str(stream, &["sec", "tls"]).unwrap_or("");
    if security == "tls" {
        let mut tls = json!({ "enabled": true });
        set_non_empty(&mut tls, "server_name", get_str(stream, &["sni"]));
        let insecure = stream
            .get("insecure")
            .and_then(Value::as_bool)
            .unwrap_or(false);
        if insecure {
            tls["insecure"] = json!(true);
        }
        if let Some(alpn) = get_str(stream, &["alpn"]) {
            if !alpn.is_empty() {
                tls["alpn"] = json!(alpn.split(',').collect::<Vec<_>>());
            }
        }
        out["tls"] = tls;
    }
}

/// Build a complete sing-box config JSON from the data model.
///
/// MVP version: SOCKS inbound from `DataStore`, the proxy's outbound,
/// a direct outbound, and optional DNS object.
pub fn build_config(proxy: &ProxyEntity, data_store: &DataStore) -> anyhow::Result<Value> {
    let mut outbound = build_outbound(proxy);
    outbound["tag"] = json!("proxy");

    let inbounds = if data_store.enable_tun_routing {
        json!([build_tun_inbound(data_store)])
    } else {
        let mut inbound = json!({
            "tag": "mixed-in",
            "type": "mixed",
            "listen": data_store.inbound_address,
            "listen_port": data_store.inbound_socks_port,
        });
        if let (Some(u), Some(p)) = (&data_store.inbound_username, &data_store.inbound_password) {
            if !p.is_empty() {
                inbound["users"] = json!([{ "username": u, "password": p }]);
            }
        }
        json!([inbound])
    };

    let mut config = json!({
        "log": { "level": data_store.log_level },
        "inbounds": inbounds,
        "outbounds": [
            outbound,
            { "tag": "direct", "type": "direct" }
        ],
        "route": {
            "rules": [],
            "final": "proxy"
        }
    });

    // Add DNS config from DataStore if enabled
    if data_store.use_dns_object {
        config["dns"] = build_dns_object(&data_store.remote_dns, data_store.enable_tun_routing);
    }

    Ok(config)
}

/// Build the TUN inbound (port of `BuildTunInbound` in ConfigBuilder.cpp).
fn build_tun_inbound(data_store: &DataStore) -> Value {
    let interface_name = format!("tun_{}", random_suffix(9));
    let mut addresses = vec![if data_store.tun_address.is_empty() {
        "172.19.0.1/24".to_string()
    } else {
        data_store.tun_address.clone()
    }];
    if data_store.vpn_ipv6 {
        addresses.push(if data_store.tun_address_6.is_empty() {
            "fdfe:dcba:9876::1/96".to_string()
        } else {
            data_store.tun_address_6.clone()
        });
    }
    json!({
        "tag": "tun-in",
        "type": "tun",
        "interface_name": interface_name,
        "auto_route": true,
        "mtu": data_store.vpn_mtu,
        "stack": data_store.vpn_implementation,
        "strict_route": data_store.vpn_strict_route,
        "address": addresses,
    })
}

/// Random lowercase suffix for the tun interface name
/// (C++ uses `GetRandomString(9, ExcludeUppercase | ExcludeDigits)`).
fn random_suffix(len: usize) -> String {
    use std::time::{SystemTime, UNIX_EPOCH};
    let mut x = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.subsec_nanos() as u64 ^ d.as_secs())
        .unwrap_or(0x9e3779b97f4a7c15);
    (0..len)
        .map(|_| {
            // xorshift64
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            (b'a' + (x % 26) as u8) as char
        })
        .collect()
}
/// Build the DNS object for sing-box config.
///
/// Port of `BuildDnsObject` from ConfigBuilder.cpp (MVP subset).
pub fn build_dns_object(address: &str, _tun_enabled: bool) -> Value {
    json!({
        "servers": [
            {
                "tag": "remote",
                "address": address,
                "detour": "proxy"
            },
            {
                "tag": "local",
                "address": "localhost",
                "detour": "direct"
            }
        ],
        "rules": []
    })
}

/// Build a config containing one outbound per profile (for URL tests).
///
/// Each outbound is tagged with the profile's ID as a string, so test
/// results can be mapped back to profiles.
pub fn build_test_config(profiles: &[&ProxyEntity]) -> (String, Vec<String>) {
    let mut outbounds = Vec::new();
    let mut tags = Vec::new();
    for p in profiles {
        let tag = p.id.to_string();
        let mut out = build_outbound(p);
        out["tag"] = json!(tag);
        outbounds.push(out);
        tags.push(tag);
    }
    let config = json!({
        "log": {},
        "outbounds": outbounds,
    });
    (config.to_string(), tags)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_build_config_basic() {
        let mut proxy = crate::model::ProxyEntity::new("direct");
        proxy.name = "test".into();
        let ds = crate::model::DataStore::default();
        let config = build_config(&proxy, &ds).unwrap();
        assert!(config["inbounds"].is_array());
        assert!(config["outbounds"].is_array());
    }

    #[test]
    fn test_build_outbound_shadowsocks() {
        let mut proxy = crate::model::ProxyEntity::new("shadowsocks");
        proxy.server_address = "example.com".into();
        proxy.server_port = 443;
        proxy.bean_cfg = Some(json!({
            "method": "aes-256-gcm",
            "pass": "secret",
        }));
        let out = build_outbound(&proxy);
        assert_eq!(out["type"], "shadowsocks");
        assert_eq!(out["server"], "example.com");
        assert_eq!(out["server_port"], 443);
        assert_eq!(out["method"], "aes-256-gcm");
        assert_eq!(out["password"], "secret");
        assert_eq!(out["udp_over_tcp"], false);
    }

    #[test]
    fn test_build_outbound_vmess_ws_tls() {
        let mut proxy = crate::model::ProxyEntity::new("vmess");
        proxy.bean_cfg = Some(json!({
            "id": "00000000-0000-4000-8000-000000000000",
            "aid": 0,
            "sec": "auto",
            "stream": {
                "net": "ws",
                "sec": "tls",
                "sni": "cdn.example.com",
                "host": "cdn.example.com",
                "path": "/ws",
            }
        }));
        let out = build_outbound(&proxy);
        assert_eq!(out["uuid"], "00000000-0000-4000-8000-000000000000");
        assert_eq!(out["transport"]["type"], "ws");
        assert_eq!(out["transport"]["path"], "/ws");
        assert_eq!(out["transport"]["headers"]["Host"], "cdn.example.com");
        assert_eq!(out["tls"]["enabled"], true);
        assert_eq!(out["tls"]["server_name"], "cdn.example.com");
    }

    #[test]
    fn test_build_test_config_tags() {
        let mut p1 = crate::model::ProxyEntity::new("direct");
        p1.id = 7;
        let (config, tags) = build_test_config(&[&p1]);
        assert_eq!(tags, vec!["7".to_string()]);
        assert!(config.contains("\"tag\":\"7\""));
    }

    #[test]
    fn test_build_outbound_hysteria2() {
        let mut proxy = crate::model::ProxyEntity::new("hysteria2");
        proxy.server_address = "hy2.example.com".into();
        proxy.server_port = 1443;
        proxy.bean_cfg = Some(json!({
            "password": "secret-uuid",
            "sni": "hy2.example.com",
            "allowInsecure": true,
            "uploadMbps": 3000,
            "downloadMbps": 3000,
            "obfsPassword": "obfs-pass",
        }));
        let out = build_outbound(&proxy);
        assert_eq!(out["type"], "hysteria2");
        assert_eq!(out["password"], "secret-uuid");
        assert_eq!(out["up_mbps"], 3000);
        assert_eq!(out["down_mbps"], 3000);
        assert_eq!(out["tls"]["enabled"], true);
        assert_eq!(out["tls"]["insecure"], true);
        assert_eq!(out["tls"]["alpn"], "h3");
        assert_eq!(out["obfs"]["type"], "salamander");
        assert_eq!(out["obfs"]["password"], "obfs-pass");
        // Bean-internal keys must not leak into the outbound.
        assert!(out.get("_v").is_none());
        assert!(out.get("allowInsecure").is_none());
        assert!(out.get("uploadMbps").is_none());
    }

    #[test]
    fn test_build_outbound_hysteria2_server_ports() {
        let mut proxy = crate::model::ProxyEntity::new("hysteria2");
        proxy.bean_cfg = Some(json!({
            "password": "p",
            "server_ports": ["443", "500:600"],
            "hop_interval": "30s",
        }));
        let out = build_outbound(&proxy);
        assert_eq!(out["server_ports"], json!(["443:443", "500:600"]));
        assert!(out.get("server_port").is_none());
        assert_eq!(out["hop_interval"], "30s");
    }

    #[test]
    fn test_build_outbound_tuic() {
        let mut proxy = crate::model::ProxyEntity::new("tuic");
        proxy.bean_cfg = Some(json!({
            "uuid": "00000000-0000-4000-8000-000000000000",
            "password": "p",
            "congestionControl": "bbr",
            "udpRelayMode": "quic",
            "zeroRttHandshake": true,
            "heartbeat": "10s",
            "sni": "tuic.example.com",
        }));
        let out = build_outbound(&proxy);
        assert_eq!(out["type"], "tuic");
        assert_eq!(out["uuid"], "00000000-0000-4000-8000-000000000000");
        assert_eq!(out["congestion_control"], "bbr");
        assert_eq!(out["udp_relay_mode"], "quic");
        assert_eq!(out["zero_rtt_handshake"], true);
        assert_eq!(out["heartbeat"], "10s");
        assert_eq!(out["tls"]["server_name"], "tuic.example.com");
    }

    #[test]
    fn test_build_outbound_anytls() {
        let mut proxy = crate::model::ProxyEntity::new("anytls");
        proxy.bean_cfg = Some(json!({
            "password": "p",
            "session_idle_check_interval": "30s",
            "session_idle_timeout": "30s",
            "min_idle_session": 0,
            "stream": { "sec": "tls", "sni": "a.example.com" },
        }));
        let out = build_outbound(&proxy);
        assert_eq!(out["type"], "anytls");
        assert_eq!(out["password"], "p");
        assert_eq!(out["idle_session_check_interval"], "30s");
        assert_eq!(out["tls"]["server_name"], "a.example.com");
    }
}
