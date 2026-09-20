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
/// `skip_cert` and `default_utls` come from the DataStore.
pub fn build_outbound(proxy: &ProxyEntity, skip_cert: bool, default_utls: Option<&str>) -> Value {
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
            add_stream_settings(&mut out, &bean, skip_cert, default_utls);
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
            add_stream_settings(&mut out, &bean, skip_cert, default_utls);
            // Trojan is always TLS: the GUI defaults security to "tls" on
            // import, but a hand-written bean without it must still get one.
            if proxy.r#type == "trojan" && out.get("tls").is_none() {
                let empty = json!({});
                let stream = bean.get("stream").unwrap_or(&empty);
                let mut tls = json!({ "enabled": true });
                set_non_empty(&mut tls, "server_name", get_str(stream, &["sni"]));
                out["tls"] = tls;
            }
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
            add_stream_settings(&mut out, &bean, skip_cert, default_utls);
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
            add_stream_settings(&mut out, &bean, skip_cert, default_utls);
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
            add_stream_settings(&mut out, &bean, skip_cert, default_utls);
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

/// `add_network`: sing-box `network` only accepts tcp/udp (the C++
/// NetworkEnum is exactly {tcp, udp}); anything else is a transport and
/// belongs to the stream settings, not here.
fn add_network(out: &mut Value, bean: &Value) {
    if let Some(net) = get_str(bean, &["network"]) {
        if net == "udp" {
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

/// Port of `V2rayStreamSettings::BuildStreamSettingsSingBox`:
/// ws/http/grpc/httpupgrade/xhttp transports, TCP+headerType-http, and TLS
/// with insecure/certificate/alpn/reality/utls/fragment. `skip_cert` and
/// `default_utls` come from the DataStore.
///
/// Tolerates both key spellings: on-disk GUI beans use `net`/`sec`,
/// our share-link parser writes `network`/`tls`.
fn add_stream_settings(out: &mut Value, bean: &Value, skip_cert: bool, default_utls: Option<&str>) {
    let empty = json!({});
    let stream = bean.get("stream").unwrap_or(&empty);

    let network = get_str(stream, &["net", "network"]).unwrap_or("tcp");
    let path = get_str(stream, &["path"]).unwrap_or("");
    let host = get_str(stream, &["host"]).unwrap_or("");

    if !network.is_empty() && network != "tcp" {
        let mut transport = json!({ "type": network });
        match network {
            "ws" => {
                // "?ed=N" in the path means early data.
                let (path_no_ed, ed) = match path.split_once("?ed=") {
                    Some((p, n)) => (p, n.parse::<i64>().unwrap_or(0)),
                    None => (path, 0),
                };
                set_non_empty(&mut transport, "path", Some(path_no_ed));
                let ed_len = get_i64(stream, &["ed_len"], 0);
                let (ed, ed_name) = if ed > 0 {
                    (ed, "Sec-WebSocket-Protocol".to_string())
                } else {
                    (ed_len, str_or(stream, &["ed_name"], "").to_string())
                };
                if ed > 0 {
                    transport["max_early_data"] = json!(ed);
                    transport["early_data_header_name"] = json!(ed_name);
                }
                if !host.is_empty() {
                    transport["headers"] = json!({ "Host": host });
                }
            }
            "grpc" => {
                set_non_empty(&mut transport, "service_name", Some(path));
            }
            "http" => {
                set_non_empty(&mut transport, "path", Some(path));
                let method = str_or(stream, &["method"], "").to_uppercase();
                set_non_empty(&mut transport, "method", Some(&method));
                if !host.is_empty() {
                    transport["host"] = json!(host.split(',').collect::<Vec<_>>());
                }
            }
            "httpupgrade" | "xhttp" => {
                set_non_empty(&mut transport, "path", Some(path));
                set_non_empty(&mut transport, "host", Some(host));
                if network == "xhttp" {
                    if let Some(mode) = get_str(stream, &["xhttp_mode"]) {
                        if !mode.is_empty() {
                            transport["mode"] = json!(mode);
                        }
                    }
                }
            }
            _ => {}
        }
        out["transport"] = transport;
    } else if get_str(stream, &["h_type"]) == Some("http") {
        // TCP with headerType=http masquerading.
        out["transport"] = json!({
            "type": "http",
            "method": "GET",
            "path": path,
            "headers": { "Host": host.split(',').collect::<Vec<_>>() },
        });
    }

    let security = get_str(stream, &["sec", "tls"]).unwrap_or("");
    if security == "tls" {
        let mut tls = json!({ "enabled": true });
        set_non_empty(&mut tls, "server_name", get_str(stream, &["sni"]));
        set_non_empty(&mut tls, "certificate", get_str(stream, &["cert"]));
        let insecure = stream
            .get("insecure")
            .and_then(Value::as_bool)
            .unwrap_or(false);
        if insecure || skip_cert {
            tls["insecure"] = json!(true);
        }
        if let Some(alpn) = get_str(stream, &["alpn"]) {
            if !alpn.is_empty() {
                tls["alpn"] = json!(alpn.split(',').collect::<Vec<_>>());
            }
        }
        let pbk = get_str(stream, &["pbk"]).unwrap_or("");
        let mut fp = get_str(stream, &["utls"]).unwrap_or("");
        if !pbk.is_empty() {
            let sid = str_or(stream, &["sid"], "");
            tls["reality"] = json!({
                "enabled": true,
                "public_key": pbk,
                "short_id": sid.split(',').next().unwrap_or(""),
            });
            if fp.is_empty() {
                fp = default_utls.unwrap_or("random");
            }
        }
        if !fp.is_empty() {
            tls["utls"] = json!({ "enabled": true, "fingerprint": fp });
        }
        if get_bool(stream, &["tls_frag"], false) {
            tls["fragment"] = json!(true);
            set_non_empty(
                &mut tls,
                "fragment_fallback_delay",
                get_str(stream, &["tls_frag_fall_delay"]),
            );
        }
        if get_bool(stream, &["tls_record_frag"], false) {
            tls["record_fragment"] = json!(true);
        }
        out["tls"] = tls;
    }

    // vmess/vless carry packet_encoding next to the transport.
    if matches!(out["type"].as_str(), Some("vmess") | Some("vless")) {
        set_non_empty(out, "packet_encoding", get_str(stream, &["pac_enc"]));
    }
}

/// Build a complete sing-box config JSON from the data model.
///
/// MVP version: SOCKS inbound from `DataStore`, the proxy's outbound,
/// a direct outbound, and optional DNS object.
pub fn build_config(proxy: &ProxyEntity, data_store: &DataStore) -> anyhow::Result<Value> {
    build_config_with_route(proxy, data_store, None, None)
}

/// Build a full sing-box config, applying `chain` as the routing section.
///
/// Passing `None` yields a bare "everything through the proxy" route, which is
/// what [`build_config`] does. The GUI always has a chain selected
/// (`current_route_id`), so the TUI should pass one too — otherwise routing
/// rules the user configured in the GUI are silently ignored.
///
/// `profiles` is consulted for rules that route through a *specific profile*
/// (outbound id >= 0); those outbounds are appended with `r-N-c-<id>` tags,
/// mirroring `BuildChainInternal` in ConfigBuilder.cpp.
pub fn build_config_with_route(
    proxy: &ProxyEntity,
    data_store: &DataStore,
    chain: Option<&crate::model::RoutingChain>,
    profiles: Option<&std::collections::HashMap<i32, ProxyEntity>>,
) -> anyhow::Result<Value> {
    let skip_cert = data_store.skip_cert;
    let default_utls = data_store.utls_fingerprint.as_deref();
    let mut outbound = build_outbound(proxy, skip_cert, default_utls);
    outbound["tag"] = json!("proxy");

    let inbounds = if data_store.enable_tun_routing {
        json!([build_tun_inbound(data_store, chain)])
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

    let mut outbounds = vec![
        outbound,
        json!({ "tag": "direct", "type": "direct" }),
        json!({ "tag": "block", "type": "block" }),
    ];

    // Rules may route some traffic through other profiles (outbound id >= 0).
    // Resolve those into real outbounds and map the ids to their tags.
    let mut outbound_map = std::collections::HashMap::new();
    outbound_map.insert(crate::model::OUTBOUND_PROXY, "proxy".to_string());
    outbound_map.insert(crate::model::OUTBOUND_DIRECT, "direct".to_string());
    outbound_map.insert(crate::model::OUTBOUND_BLOCK, "block".to_string());
    if let Some(chain) = chain {
        for (n, id) in chain.used_profile_outbounds().into_iter().enumerate() {
            let ent = profiles.and_then(|ps| ps.get(&id));
            let Some(ent) = ent else {
                anyhow::bail!(
                    "The routing profile is referencing outbounds that no longer exists, \
                     consider revising your settings"
                );
            };
            let tag = format!("r-{n}-c-{id}");
            let mut o = build_outbound(ent, skip_cert, default_utls);
            o["tag"] = json!(tag.clone());
            outbounds.push(o);
            outbound_map.insert(id, tag);
        }
    }

    let mut config = json!({
        "log": { "level": data_store.log_level },
        "inbounds": inbounds,
        "outbounds": outbounds,
        "route": build_route(data_store, chain, &outbound_map),
    });

    // Add DNS config from DataStore if enabled
    if data_store.use_dns_object {
        config["dns"] = build_dns_object(&data_store.remote_dns, data_store.enable_tun_routing);
    }

    if let Some(experimental) = build_experimental(data_store) {
        config["experimental"] = experimental;
    }

    Ok(config)
}

/// Build the `experimental` object.
///
/// `clash_api` is what makes `ListConnections` work: the core looks up a
/// `ClashServer` in the box context and errors with "no clash server found"
/// without it. The GUI emits the section whenever the Clash API port is set
/// *or* connection statistics are enabled (`BuildConfig` in ConfigBuilder.cpp),
/// using a `default_mode` placeholder in the latter case so the server is
/// still constructed.
fn build_experimental(data_store: &DataStore) -> Option<Value> {
    let api_enabled = data_store.core_box_clash_api > 0;
    if !api_enabled && !data_store.connection_statistics {
        return None;
    }
    let clash_api = if api_enabled {
        json!({
            "external_controller": format!(
                "{}:{}",
                data_store.core_box_clash_listen_addr, data_store.core_box_clash_api
            ),
            "secret": data_store.core_box_clash_api_secret.clone().unwrap_or_default(),
            "external_ui": "dashboard",
        })
    } else {
        json!({ "default_mode": "" })
    };
    Some(json!({ "clash_api": clash_api }))
}

/// Build the `route` object: the active chain's rules, its rule sets, the
/// sniff/resolve prelude rules, the tun-split process rules and the chain's
/// default outbound.
fn build_route(
    data_store: &DataStore,
    chain: Option<&crate::model::RoutingChain>,
    outbound_map: &std::collections::HashMap<i32, String>,
) -> Value {
    use crate::model::{rule_set_json, ADBLOCK_RULE_SET, ADBLOCK_TAG};

    let mut rules: Vec<Value> = Vec::new();
    let mut rule_sets: Vec<Value> = Vec::new();

    // The prelude rules the GUI prepends to the active chain
    // (BuildConfigSingBox): resolve by domain strategy, then sniffing.
    if !data_store.domain_strategy.is_empty() {
        rules.push(json!({
            "action": "resolve",
            "inbound": ["mixed-in", "tun-in"],
            "strategy": data_store.domain_strategy,
        }));
    }
    if data_store.sniffing_mode != 0 {
        rules.push(json!({
            "action": "sniff",
            "inbound": ["mixed-in", "tun-in"],
        }));
    }

    if let Some(chain) = chain {
        for rs in chain.used_rule_sets() {
            if let Some(json) = rule_set_json(&rs, data_store.ruleset_mirror) {
                rule_sets.push(json);
            }
        }
        rules.extend(chain.to_route_rules(outbound_map, data_store.adblock_enable));
    }
    if data_store.adblock_enable
        && !rule_sets
            .iter()
            .any(|rs| rs["tag"] == json!(ADBLOCK_TAG))
    {
        if let Some(mut json) = rule_set_json(ADBLOCK_RULE_SET, data_store.ruleset_mirror) {
            json["tag"] = json!(ADBLOCK_TAG);
            rule_sets.push(json);
        }
    }

    // Per-process routing only applies to the TUN inbound.
    if data_store.enable_tun_routing {
        let split = &data_store.tun_split;
        if !split.proxy.is_empty() {
            rules.push(json!({
                "action": "route", "outbound": "proxy", "process_path": split.proxy
            }));
        }
        if !split.direct.is_empty() {
            rules.push(json!({
                "action": "route", "outbound": "direct", "process_path": split.direct
            }));
        }
        if !split.block.is_empty() {
            rules.push(json!({ "action": "reject", "process_path": split.block }));
        }
    }

    let final_tag = match chain.map(|c| c.default_outbound_id) {
        Some(crate::model::OUTBOUND_DIRECT) => "direct",
        Some(crate::model::OUTBOUND_BLOCK) => "block",
        _ => "proxy",
    };

    let mut route = json!({
        "rules": rules,
        "final": final_tag,
    });
    // The GUI only auto-detects the interface in TUN mode.
    if data_store.enable_tun_routing {
        route["auto_detect_interface"] = json!(true);
    }
    // Needed for the Process column of the connections view.
    if data_store.connection_statistics {
        route["find_process"] = json!(true);
    }
    if !rule_sets.is_empty() {
        route["rule_set"] = json!(rule_sets);
    }
    route
}

/// Build the TUN inbound (port of `BuildTunInbound` in ConfigBuilder.cpp).
fn build_tun_inbound(data_store: &DataStore, chain: Option<&crate::model::RoutingChain>) -> Value {
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
    // The GUI excludes the configured addresses plus the chain's direct
    // destinations from the tun routes.
    let mut exclude: Vec<String> = data_store.route_exclude_addrs.clone();
    if let Some(chain) = chain {
        for rule in &chain.rules {
            if rule.outbound_id != crate::model::OUTBOUND_DIRECT {
                continue;
            }
            for cidr in &rule.ip_cidr {
                if !cidr.trim().is_empty() {
                    exclude.push(cidr.clone());
                }
            }
        }
    }
    let exclude_sets: Vec<String> = chain
        .map(|c| {
            c.rules
                .iter()
                .filter(|r| r.outbound_id == crate::model::OUTBOUND_DIRECT)
                .flat_map(|r| r.rule_set.iter().cloned())
                .filter(|s| s.starts_with("geoip-"))
                .collect()
        })
        .unwrap_or_default();
    let mut inbound = json!({
        "tag": "tun-in",
        "type": "tun",
        "interface_name": interface_name,
        "auto_route": true,
        "mtu": data_store.vpn_mtu,
        "stack": data_store.vpn_implementation,
        "strict_route": data_store.vpn_strict_route,
        "address": addresses,
    });
    if !exclude.is_empty() {
        inbound["route_exclude_address"] = json!(exclude);
    }
    if !exclude_sets.is_empty() {
        inbound["route_exclude_address_set"] = json!(exclude_sets);
    }
    inbound
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
pub fn build_test_config(profiles: &[&ProxyEntity], data_store: &DataStore) -> (String, Vec<String>) {
    let skip_cert = data_store.skip_cert;
    let default_utls = data_store.utls_fingerprint.as_deref();
    let mut outbounds = Vec::new();
    let mut tags = Vec::new();
    for p in profiles {
        let tag = p.id.to_string();
        let mut out = build_outbound(p, skip_cert, default_utls);
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
        let out = build_outbound(&proxy, false, None);
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
        let out = build_outbound(&proxy, false, None);
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
        let (config, tags) = build_test_config(&[&p1], &crate::model::DataStore::default());
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
        let out = build_outbound(&proxy, false, None);
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
        let out = build_outbound(&proxy, false, None);
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
        let out = build_outbound(&proxy, false, None);
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
        let out = build_outbound(&proxy, false, None);
        assert_eq!(out["type"], "anytls");
        assert_eq!(out["password"], "p");
        assert_eq!(out["idle_session_check_interval"], "30s");
        assert_eq!(out["tls"]["server_name"], "a.example.com");
    }

    #[test]
    fn test_build_outbound_vless_reality() {
        let mut proxy = crate::model::ProxyEntity::new("vless");
        proxy.bean_cfg = Some(json!({
            "pass": "00000000-0000-4000-8000-000000000000",
            "flow": "xtls-rprx-vision",
            "stream": {
                "net": "tcp",
                "sec": "tls",
                "sni": "www.microsoft.com",
                "pbk": "PUBKEY",
                "sid": "ab,cd",
                "utls": "chrome",
                "pac_enc": "xudp",
            },
        }));
        let out = build_outbound(&proxy, false, None);
        assert_eq!(out["tls"]["reality"]["enabled"], true);
        assert_eq!(out["tls"]["reality"]["public_key"], "PUBKEY");
        // Only the first sid entry is used (C++ behaviour).
        assert_eq!(out["tls"]["reality"]["short_id"], "ab");
        assert_eq!(out["tls"]["utls"]["fingerprint"], "chrome");
        assert_eq!(out["packet_encoding"], "xudp");

        // Without an explicit fingerprint, reality falls back to the
        // datastore default, then "random".
        proxy.bean_cfg = Some(json!({
            "pass": "id",
            "stream": { "sec": "tls", "pbk": "PUBKEY" },
        }));
        let out = build_outbound(&proxy, false, Some("firefox"));
        assert_eq!(out["tls"]["utls"]["fingerprint"], "firefox");
        let out = build_outbound(&proxy, false, None);
        assert_eq!(out["tls"]["utls"]["fingerprint"], "random");
    }

    #[test]
    fn test_skip_cert_forces_insecure() {
        let mut proxy = crate::model::ProxyEntity::new("vmess");
        proxy.bean_cfg = Some(json!({
            "id": "id",
            "stream": { "sec": "tls", "sni": "a.example.com" },
        }));
        let out = build_outbound(&proxy, true, None);
        assert_eq!(out["tls"]["insecure"], true);
        let out = build_outbound(&proxy, false, None);
        assert!(out["tls"].get("insecure").is_none());
    }

    #[test]
    fn test_trojan_always_has_tls() {
        let mut proxy = crate::model::ProxyEntity::new("trojan");
        proxy.bean_cfg = Some(json!({ "pass": "p" }));
        let out = build_outbound(&proxy, false, None);
        assert_eq!(out["tls"]["enabled"], true);
    }

    /// A chain rule that routes through another profile (outbound id >= 0)
    /// must produce a real outbound and a tag reference, not a raw number.
    #[test]
    fn test_route_via_profile_outbound() {
        use crate::model::{RouteRule, RoutingChain};

        let mut other = crate::model::ProxyEntity::new("shadowsocks");
        other.id = 42;
        other.server_address = "other.example.com".into();
        other.server_port = 8388;
        other.bean_cfg = Some(json!({ "method": "aes-256-gcm", "pass": "pw" }));
        let profiles = std::collections::HashMap::from([(42, other)]);

        let mut chain = RoutingChain::new();
        chain.rules = vec![RouteRule {
            name: "special".into(),
            domain_suffix: vec!["special.example.com".into()],
            outbound_id: 42,
            action: "route".into(),
            ..Default::default()
        }];

        let proxy = crate::model::ProxyEntity::new("shadowsocks");
        let ds = crate::model::DataStore::default();
        let config =
            build_config_with_route(&proxy, &ds, Some(&chain), Some(&profiles)).unwrap();
        let rules = config["route"]["rules"].as_array().unwrap();
        let rule = rules.iter().find(|r| r["domain_suffix"][0] == "special.example.com").unwrap();
        assert_eq!(rule["outbound"], "r-0-c-42");
        let tags: Vec<&str> = config["outbounds"]
            .as_array()
            .unwrap()
            .iter()
            .filter_map(|o| o["tag"].as_str())
            .collect();
        assert!(tags.contains(&"r-0-c-42"), "profile outbound added: {tags:?}");

        // Referencing a profile nobody has must fail loudly, like the GUI.
        assert!(build_config_with_route(&proxy, &ds, Some(&chain), None).is_err());
    }

    /// The GUI's jsdelivr mirrors must apply to remote rule sets.
    #[test]
    fn test_ruleset_mirror_rewrite() {
        let url = "https://raw.githubusercontent.com/217heidai/adblockfilters/main/rules/adblocksingbox.srs";
        let json = crate::model::rule_set_json(url, crate::model::mirror::GITHUB).unwrap();
        assert_eq!(json["url"], url);
        let json = crate::model::rule_set_json(url, crate::model::mirror::CLOUDFLARE).unwrap();
        assert_eq!(
            json["url"],
            "https://testingcf.jsdelivr.net/gh/217heidai/adblockfilters@main/rules/adblocksingbox.srs"
        );
        // Non-raw hosts pass through untouched.
        let other = "https://github.com/x/y/raw/main/z.srs";
        let json = crate::model::rule_set_json(other, crate::model::mirror::CLOUDFLARE).unwrap();
        assert_eq!(json["url"], other);
    }
}
