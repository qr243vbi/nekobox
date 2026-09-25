//! ConfigBuilder — model → sing-box JSON config.
//!
//! Port of `src/gharqad/configs/ConfigBuilder.cpp` (`BuildConfigSingBox`,
//! `BuildTestConfig`) and the per-protocol `BuildCoreObjSingBox` in
//! `src/gharqad/configs/proxy/`.
//!
//! The ConfigBuilder takes a `ProxyEntity` (with its bean config), a
//! `DataStore`, and optionally a `RoutingChain`, and produces a
//! complete sing-box JSON config suitable for passing to the core via
//! `Start(LoadConfigReq)`.

use crate::model::{DataStore, ProxyEntity, RoutingChain};
use serde_json::{json, Value};
use std::collections::HashMap;

/// Build the sing-box outbound object for a proxy entity.
///
/// Port of `BuildCoreObjSingBox` from the per-protocol beans
/// (`ShadowSocksBean`, `VMessBean`, `TrojanVLESSBean`, ...).
/// `add_default_fields` always contributes `type`/`server`/`server_port`.
/// `skip_cert` comes from the DataStore. Fails for profile types this port
/// cannot build yet (WireGuard/AmneziaWG, chains, custom and extra-core
/// configs, SSH, Tor, Tailscale, …) instead of handing the core an outbound
/// it would reject.
pub fn build_outbound(proxy: &ProxyEntity, skip_cert: bool) -> anyhow::Result<Value> {
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
            add_stream_settings(&mut out, &bean, skip_cert);
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
            add_stream_settings(&mut out, &bean, skip_cert);
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
            add_stream_settings(&mut out, &bean, skip_cert);
        }
        "direct" | "block" | "dns" => {
            // Type-only outbounds; nothing else needed.
            return Ok(json!({ "type": sing_box_type(&proxy.r#type) }));
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
            add_stream_settings(&mut out, &bean, skip_cert);
        }
        _ => anyhow::bail!(
            "{} profiles are not supported by nekobox-tui yet",
            proxy.display_core_type()
        ),
    }

    Ok(out)
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
/// with insecure/certificate/alpn/reality/utls/fragment. `skip_cert` comes
/// from the DataStore; the global uTLS fingerprint is applied when a link is
/// imported (as in the GUI), not here.
///
/// Tolerates both key spellings: on-disk GUI beans use `net`/`sec`,
/// our share-link parser writes `network`/`tls`.
fn add_stream_settings(out: &mut Value, bean: &Value, skip_cert: bool) {
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
            // Reality needs uTLS; the core refuses it without one.
            if fp.is_empty() {
                fp = "random";
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

/// Build a complete sing-box config with no routing profile.
pub fn build_config(proxy: &ProxyEntity, data_store: &DataStore) -> anyhow::Result<Value> {
    build_config_with_route(proxy, data_store, None, None)
}

/// Build a full sing-box config, applying `chain` as the routing section.
/// Port of `BuildConfigSingBox`.
///
/// Passing `None` yields a bare "everything through the proxy" route, which is
/// what [`build_config`] does. The GUI always has a chain selected
/// (`current_route_id`), so the TUI should pass one too — otherwise routing
/// rules the user configured in the GUI are silently ignored.
///
/// `profiles` is consulted for rules that route through a *specific profile*
/// (outbound id >= 0); those outbounds are appended with `r-N-c-<id>` tags,
/// mirroring `BuildChainInternal` in ConfigBuilder.cpp.
///
/// The special-proxy mode comes from `data_store.spmode_vpn` (TUN inbound)
/// and `data_store.spmode_system_proxy` (`set_system_proxy` on the local
/// inbound).
pub fn build_config_with_route(
    proxy: &ProxyEntity,
    data_store: &DataStore,
    chain: Option<&RoutingChain>,
    profiles: Option<&HashMap<i32, ProxyEntity>>,
) -> anyhow::Result<Value> {
    use crate::model::{OUTBOUND_BLOCK, OUTBOUND_DIRECT, OUTBOUND_PROXY};

    let ds = data_store;
    let mut outbound = build_outbound(proxy, ds.skip_cert)?;
    outbound["tag"] = json!("proxy");

    // Domains the direct DNS server resolves: the proxy servers themselves
    // (looking them up through the proxy cannot work) and whatever the
    // routing profile sends direct.
    let mut direct_dns = DirectDns::default();
    direct_dns.add_server(&proxy.server_address);
    let mut direct_ip_sets: Vec<String> = Vec::new();
    let mut direct_ip_cidrs: Vec<String> = Vec::new();
    if let Some(chain) = chain {
        for site in chain.direct_sites() {
            direct_dns.add_site(&site);
        }
        for ip in chain.direct_ips() {
            if let Some(set) = ip.strip_prefix("ruleset:") {
                direct_ip_sets.push(set.to_string());
            } else if let Some(cidr) = ip.strip_prefix("ip:") {
                direct_ip_cidrs.push(cidr.to_string());
            }
        }
    }

    // --- Inbounds ---
    let mut inbounds: Vec<Value> = Vec::new();
    if (1..=65535).contains(&ds.inbound_socks_port) && ds.proxy_inbound_enabled() {
        let mut inbound = json!({
            "tag": "mixed-in",
            "type": ds.inbound_type_name(),
            "listen": ds.inbound_address,
            "listen_port": ds.inbound_socks_port,
            "set_system_proxy": ds.spmode_system_proxy,
        });
        if let (Some(u), Some(p)) = (&ds.inbound_username, &ds.inbound_password) {
            if !u.is_empty() && !p.is_empty() {
                inbound["users"] = json!([{ "username": u, "password": p }]);
            }
        }
        inbounds.push(inbound);
    }
    if ds.spmode_vpn {
        inbounds.push(build_tun_inbound(ds, &direct_ip_sets, &direct_ip_cidrs));
    }
    // Rules the GUI puts in front of the routing profile, in its order.
    let mut prelude: Vec<Value> = Vec::new();
    if ds.enable_dns_server {
        prelude.push(json!({ "action": "sniff", "inbound": ["dns-in"] }));
        prelude.push(json!({ "action": "hijack-dns", "inbound": ["dns-in"] }));
    }
    if ds.enable_redirect {
        inbounds.insert(
            0,
            json!({
                "tag": "hijack",
                "type": "direct",
                "listen": ds.redirect_listen_address,
                "listen_port": ds.redirect_listen_port,
            }),
        );
        prelude.insert(
            0,
            json!({ "action": "sniff", "inbound": ["hijack"], "override_destination": true }),
        );
    }
    if let Some(custom) = serde_json::from_str::<Value>(&ds.custom_inbound)
        .ok()
        .and_then(|v| v.get("inbounds").and_then(Value::as_array).cloned())
    {
        inbounds.extend(custom);
    }
    if !ds.domain_strategy.is_empty() {
        prelude.insert(
            0,
            json!({
                "action": "resolve",
                "inbound": ["mixed-in", "tun-in"],
                "strategy": ds.domain_strategy,
            }),
        );
    }
    if ds.sniffing_mode != 0 {
        prelude.insert(0, json!({ "action": "sniff", "inbound": ["mixed-in", "tun-in"] }));
    }

    // --- Outbounds ---
    let mut outbounds = vec![
        outbound,
        json!({ "tag": "direct", "type": "direct" }),
        json!({ "tag": "block", "type": "block" }),
    ];

    // Rules may route some traffic through other profiles (outbound id >= 0).
    // Resolve those into real outbounds and map the ids to their tags.
    let mut outbound_map = HashMap::new();
    outbound_map.insert(OUTBOUND_PROXY, "proxy".to_string());
    outbound_map.insert(OUTBOUND_DIRECT, "direct".to_string());
    outbound_map.insert(OUTBOUND_BLOCK, "block".to_string());
    if let Some(chain) = chain {
        for (n, id) in chain.used_profile_outbounds().into_iter().enumerate() {
            let Some(ent) = profiles.and_then(|ps| ps.get(&id)) else {
                anyhow::bail!(
                    "The routing profile is referencing outbounds that no longer exists, \
                     consider revising your settings"
                );
            };
            let tag = format!("r-{n}-c-{id}");
            let mut o = build_outbound(ent, ds.skip_cert)?;
            o["tag"] = json!(tag.clone());
            outbounds.push(o);
            outbound_map.insert(id, tag);
            direct_dns.add_server(&ent.server_address);
        }
    }

    // --- Route ---
    let mut rules = prelude;
    let mut rule_set_names: Vec<String> = Vec::new();
    if let Some(chain) = chain {
        rules.extend(chain.to_route_rules(&outbound_map, ds.adblock_enable));
        rule_set_names.extend(chain.used_rule_sets());
    }
    if ds.spmode_vpn {
        let split = &ds.tun_split;
        if !split.proxy.is_empty() {
            rules.push(json!({ "action": "route", "outbound": "proxy", "process_path": split.proxy }));
        }
        if !split.direct.is_empty() {
            rules.push(json!({ "action": "route", "outbound": "direct", "process_path": split.direct }));
        }
        if !split.block.is_empty() {
            rules.push(json!({ "action": "reject", "process_path": split.block }));
        }
    }
    let hijack = HijackRules::from_data_store(ds);
    for set in &hijack.rule_sets {
        if !rule_set_names.contains(set) {
            rule_set_names.push(set.clone());
        }
    }
    let mut rule_sets = Vec::new();
    for name in &rule_set_names {
        match crate::model::rule_set_json(name, ds.ruleset_mirror) {
            Some(json) => rule_sets.push(json),
            // sing-box would only say "rule-set not found" at start.
            None => anyhow::bail!("unknown rule set \"{name}\" in the routing profile"),
        }
    }
    if ds.adblock_enable && !rule_set_names.iter().any(|n| n == crate::model::ADBLOCK_TAG) {
        if let Some(json) = crate::model::rule_set_json(crate::model::ADBLOCK_TAG, ds.ruleset_mirror) {
            rule_sets.push(json);
        }
    }

    let final_tag = match chain.map(|c| c.default_outbound_id) {
        Some(OUTBOUND_DIRECT) => "direct",
        Some(OUTBOUND_BLOCK) => "block",
        _ => "proxy",
    };
    let mut route = json!({
        "rules": rules,
        "rule_set": rule_sets,
        "final": final_tag,
        "default_domain_resolver": {
            "server": "dns-direct",
            "strategy": ds.outbound_domain_strategy,
        },
    });
    if ds.spmode_vpn {
        route["auto_detect_interface"] = json!(true);
    }
    // Needed for the Process column of the connections view.
    if ds.connection_statistics {
        route["find_process"] = json!(true);
    }

    // --- DNS ---
    let dns = build_dns(ds, &direct_dns, &hijack, &mut inbounds);

    let mut config = json!({
        "log": { "level": ds.log_level },
        "certificate": { "store": if ds.use_mozilla_certs { "mozilla" } else { "system" } },
        "dns": dns,
        "inbounds": inbounds,
        "outbounds": outbounds,
        "route": route,
    });
    if ds.enable_ntp {
        config["ntp"] = json!({
            "enabled": true,
            "server": ds.ntp_server_address.clone().unwrap_or_default(),
            "server_port": ds.ntp_server_port,
            "interval": ds.ntp_interval.clone().unwrap_or_default(),
        });
    }
    if let Some(experimental) = build_experimental(ds) {
        config["experimental"] = experimental;
    }

    Ok(config)
}

/// What the direct DNS server must resolve (`directDomains` & co. in
/// `BuildConfigSingBox`).
#[derive(Default)]
struct DirectDns {
    domains: Vec<String>,
    suffixes: Vec<String>,
    keywords: Vec<String>,
    regexes: Vec<String>,
    rule_sets: Vec<String>,
}

impl DirectDns {
    /// A proxy server address: resolved directly unless it is an IP.
    fn add_server(&mut self, address: &str) {
        if !address.is_empty() && address.parse::<std::net::IpAddr>().is_err() {
            self.domains.push(address.to_string());
        }
    }

    /// A `kind:value` entry from [`RoutingChain::direct_sites`].
    fn add_site(&mut self, site: &str) {
        let Some((kind, value)) = site.split_once(':') else {
            return;
        };
        let list = match kind {
            "ruleset" => &mut self.rule_sets,
            "domain" => &mut self.domains,
            "suffix" => &mut self.suffixes,
            "keyword" => &mut self.keywords,
            "regex" => &mut self.regexes,
            _ => return,
        };
        list.push(value.to_string());
    }

    fn is_empty(&self) -> bool {
        self.domains.is_empty()
            && self.suffixes.is_empty()
            && self.keywords.is_empty()
            && self.regexes.is_empty()
            && self.rule_sets.is_empty()
    }
}

/// Domains the local DNS server answers itself (`dns_server_rules`).
#[derive(Default)]
struct HijackRules {
    enabled: bool,
    domains: Vec<String>,
    suffixes: Vec<String>,
    regexes: Vec<String>,
    rule_sets: Vec<String>,
}

impl HijackRules {
    fn from_data_store(ds: &DataStore) -> Self {
        let mut out = Self {
            enabled: ds.enable_dns_server,
            ..Default::default()
        };
        if !out.enabled {
            return out;
        }
        for rule in &ds.dns_server_rules {
            if let Some(v) = rule.strip_prefix("ruleset:") {
                out.rule_sets.push(v.to_string());
            } else if let Some(v) = rule.strip_prefix("domain:") {
                out.domains.push(v.to_string());
            } else if let Some(v) = rule.strip_prefix("suffix:") {
                out.suffixes.push(v.to_string());
            } else if let Some(v) = rule.strip_prefix("regex:") {
                out.regexes.push(v.to_string());
            }
        }
        out
    }
}

/// The `dns` section (the DNS half of `BuildConfigSingBox`): remote DNS
/// through the proxy, direct DNS for the proxy servers and direct routes, a
/// local resolver for both of those, plus the hijack server's inbound. With
/// `use_dns_object` the user's own object replaces all of it.
fn build_dns(
    ds: &DataStore,
    direct: &DirectDns,
    hijack: &HijackRules,
    inbounds: &mut Vec<Value>,
) -> Value {
    let mut servers: Vec<Value> = Vec::new();
    let mut rules: Vec<Value> = Vec::new();

    let mut remote = build_dns_object(&ds.remote_dns);
    remote["tag"] = json!("dns-remote");
    remote["domain_resolver"] = json!("dns-local");
    remote["detour"] = json!("proxy");
    servers.push(remote);

    let mut direct_server = build_dns_object(&ds.direct_dns);
    direct_server["tag"] = json!("dns-direct");
    direct_server["domain_resolver"] = json!("dns-local");
    if ds.dns_final_out_direct {
        servers.insert(0, direct_server);
    } else {
        servers.push(direct_server);
    }

    for (query_type, answer) in [("A", "localhost. IN A 127.0.0.1"), ("AAAA", "localhost. IN AAAA ::1")] {
        rules.push(json!({
            "domain": "localhost",
            "action": "predefined",
            "query_type": query_type,
            "rcode": "NOERROR",
            "answer": answer,
        }));
    }

    if hijack.enabled {
        let mut answers = vec![("A", format!("* IN A {}", ds.dns_v4_resp))];
        if !ds.dns_v6_resp.is_empty() {
            answers.push(("AAAA", format!("* IN AAAA {}", ds.dns_v6_resp)));
        }
        for (query_type, answer) in answers {
            rules.push(json!({
                "rule_set": hijack.rule_sets,
                "domain": hijack.domains,
                "domain_suffix": hijack.suffixes,
                "domain_regex": hijack.regexes,
                "query_type": query_type,
                "action": "predefined",
                "rcode": "NOERROR",
                "answer": answer,
            }));
        }
        inbounds.insert(
            0,
            json!({
                "tag": "dns-in",
                "type": "direct",
                "listen": if ds.dns_server_listen_lan { "0.0.0.0" } else { "127.1.1.1" },
                "listen_port": ds.dns_server_listen_port,
            }),
        );
    }

    let mut dns = json!({});
    if ds.fake_dns {
        servers.push(json!({
            "tag": "dns-fake",
            "type": "fakeip",
            "inet4_range": "198.18.0.0/15",
            "inet6_range": "fc00::/18",
        }));
        rules.push(json!({ "query_type": ["A", "AAAA"], "action": "route", "server": "dns-fake" }));
        dns["independent_cache"] = json!(true);
    }

    if !direct.is_empty() {
        rules.push(json!({
            "rule_set": direct.rule_sets,
            "domain": direct.domains,
            "domain_suffix": direct.suffixes,
            "domain_keyword": direct.keywords,
            "domain_regex": direct.regexes,
            "action": "route",
            "server": "dns-direct",
        }));
    }

    let underlying = ds
        .core_box_underlying_dns
        .as_deref()
        .filter(|s| !s.is_empty())
        .unwrap_or("local");
    let mut local = build_dns_object(underlying);
    local["tag"] = json!("dns-local");
    servers.push(local);

    dns["servers"] = json!(servers);
    dns["rules"] = json!(rules);
    if !ds.dns_final_out_direct {
        dns["final"] = json!("dns-remote");
    }

    if ds.use_dns_object {
        // Like `QString2QJsonObject`: anything but a JSON object is empty.
        return ds
            .dns_object
            .as_deref()
            .and_then(|s| serde_json::from_str::<Value>(s).ok())
            .filter(Value::is_object)
            .unwrap_or_else(|| json!({}));
    }
    dns
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

/// Build the TUN inbound (port of `BuildTunInbound` in ConfigBuilder.cpp).
/// With `enable_tun_routing`, what the routing profile sends direct by IP
/// bypasses the TUN routes altogether.
fn build_tun_inbound(data_store: &DataStore, direct_ip_sets: &[String], direct_ip_cidrs: &[String]) -> Value {
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
    let mut exclude: Vec<String> = data_store.route_exclude_addrs.clone();
    let mut exclude_sets: Vec<String> = Vec::new();
    if data_store.enable_tun_routing {
        exclude.extend(direct_ip_cidrs.iter().filter(|c| !c.trim().is_empty()).cloned());
        exclude_sets.extend(direct_ip_sets.iter().cloned());
    }
    let mut inbound = json!({
        "tag": "tun-in",
        "type": "tun",
        "interface_name": interface_name,
        "auto_route": true,
        "mtu": data_store.vpn_mtu,
        "stack": data_store.vpn_implementation,
        "strict_route": data_store.vpn_strict_route,
        "address": addresses,
        "route_exclude_address": exclude,
    });
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

/// A DNS server object from a DNS address setting. Port of
/// `BuildDnsObject`: `local…`, `dhcp://<iface|auto>`, or
/// `[tcp|tls|udp|quic|https|h3]://host[;port][/path]` (the GUI separates the
/// port with `;`).
pub fn build_dns_object(address: &str) -> Value {
    if address.starts_with("local") {
        return json!({ "type": "local" });
    }
    if let Some(iface) = address.strip_prefix("dhcp://") {
        let iface = if iface == "auto" { "" } else { iface };
        return json!({ "type": "dhcp", "interface": iface });
    }
    let (kind, mut addr) = [
        ("tcp", "tcp://"),
        ("tls", "tls://"),
        ("udp", "udp://"),
        ("quic", "quic://"),
        ("https", "https://"),
        ("h3", "h3://"),
    ]
    .into_iter()
    .find_map(|(kind, prefix)| address.strip_prefix(prefix).map(|rest| (kind, rest.to_string())))
    .unwrap_or(("udp", address.to_string()));
    let mut path = String::new();
    if matches!(kind, "https" | "h3") {
        if let Some(idx) = addr.find('/') {
            path = addr.rsplit('/').next().unwrap_or("").to_string();
            addr.truncate(idx);
        }
    }
    let mut port = None;
    if let Some(idx) = addr.rfind(';') {
        if let Ok(p) = addr[idx + 1..].parse::<u16>() {
            port = Some(p);
            addr.truncate(idx);
        }
    }
    let mut out = json!({ "type": kind, "server": addr });
    if let Some(p) = port {
        out["server_port"] = json!(p);
    }
    if !path.is_empty() {
        out["path"] = json!(path);
    }
    out
}

/// A URL/speed test config: one outbound per profile, tagged with the
/// profile id so results map back.
pub struct TestConfig {
    pub json: String,
    pub tags: Vec<String>,
    /// Profiles left out because no outbound could be built for them.
    pub invalid: Vec<(i32, String)>,
}

/// Build a config containing one outbound per profile (for URL/speed
/// tests). Like `BuildTestConfig`, it binds to the physical interface so a
/// running TUN does not carry the test traffic, and profiles that cannot be
/// built are reported instead of breaking the whole batch.
pub fn build_test_config(profiles: &[&ProxyEntity], data_store: &DataStore) -> TestConfig {
    let mut outbounds = vec![json!({ "type": "direct", "tag": "direct" })];
    let mut tags = Vec::new();
    let mut invalid = Vec::new();
    for p in profiles {
        match build_outbound(p, data_store.skip_cert) {
            Ok(mut out) => {
                let tag = p.id.to_string();
                out["tag"] = json!(tag);
                outbounds.push(out);
                tags.push(tag);
            }
            Err(e) => invalid.push((p.id, format!("{e:#}"))),
        }
    }
    let config = json!({
        "log": { "level": "warn" },
        "outbounds": outbounds,
        "route": { "auto_detect_interface": true, "final": "direct" },
    });
    TestConfig {
        json: config.to_string(),
        tags,
        invalid,
    }
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
        let out = build_outbound(&proxy, false).unwrap();
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
        let out = build_outbound(&proxy, false).unwrap();
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
        let mut wg = crate::model::ProxyEntity::new("wireguard");
        wg.id = 8;
        let tc = build_test_config(&[&p1, &wg], &crate::model::DataStore::default());
        assert_eq!(tc.tags, vec!["7".to_string()]);
        assert!(tc.json.contains("\"tag\":\"7\""));
        // An unsupported profile is reported, not emitted as a broken
        // outbound that would fail the whole batch.
        assert_eq!(tc.invalid.len(), 1);
        assert_eq!(tc.invalid[0].0, 8);
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
        let out = build_outbound(&proxy, false).unwrap();
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
        let out = build_outbound(&proxy, false).unwrap();
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
        let out = build_outbound(&proxy, false).unwrap();
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
        let out = build_outbound(&proxy, false).unwrap();
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
        let out = build_outbound(&proxy, false).unwrap();
        assert_eq!(out["tls"]["reality"]["enabled"], true);
        assert_eq!(out["tls"]["reality"]["public_key"], "PUBKEY");
        // Only the first sid entry is used (C++ behaviour).
        assert_eq!(out["tls"]["reality"]["short_id"], "ab");
        assert_eq!(out["tls"]["utls"]["fingerprint"], "chrome");
        assert_eq!(out["packet_encoding"], "xudp");

        // Reality requires uTLS: without a fingerprint the GUI (and now the
        // TUI) falls back to "random" — the core refuses reality otherwise.
        proxy.bean_cfg = Some(json!({
            "pass": "id",
            "stream": { "sec": "tls", "pbk": "PUBKEY", "utls": "" },
        }));
        let out = build_outbound(&proxy, false).unwrap();
        assert_eq!(out["tls"]["utls"]["fingerprint"], "random");
    }

    #[test]
    fn test_skip_cert_forces_insecure() {
        let mut proxy = crate::model::ProxyEntity::new("vmess");
        proxy.bean_cfg = Some(json!({
            "id": "id",
            "stream": { "sec": "tls", "sni": "a.example.com" },
        }));
        let out = build_outbound(&proxy, true).unwrap();
        assert_eq!(out["tls"]["insecure"], true);
        let out = build_outbound(&proxy, false).unwrap();
        assert!(out["tls"].get("insecure").is_none());
    }

    #[test]
    fn test_trojan_always_has_tls() {
        let mut proxy = crate::model::ProxyEntity::new("trojan");
        proxy.bean_cfg = Some(json!({ "pass": "p" }));
        let out = build_outbound(&proxy, false).unwrap();
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

    /// The special-proxy mode lives in the config: the local inbound sets the
    /// system proxy itself, and TUN adds an inbound without dropping it.
    #[test]
    fn test_spmode_in_config() {
        let proxy = crate::model::ProxyEntity::new("socks");
        let mut ds = crate::model::DataStore {
            spmode_system_proxy: true,
            ..Default::default()
        };
        let config = build_config(&proxy, &ds).unwrap();
        assert_eq!(config["inbounds"][0]["tag"], "mixed-in");
        assert_eq!(config["inbounds"][0]["set_system_proxy"], true);

        ds.spmode_system_proxy = false;
        ds.spmode_vpn = true;
        let config = build_config(&proxy, &ds).unwrap();
        let tags: Vec<&str> = config["inbounds"]
            .as_array()
            .unwrap()
            .iter()
            .filter_map(|i| i["tag"].as_str())
            .collect();
        assert_eq!(tags, vec!["mixed-in", "tun-in"]);
        assert_eq!(config["inbounds"][0]["set_system_proxy"], false);
        assert_eq!(config["route"]["auto_detect_interface"], true);

        // No local inbound configured: none is generated.
        ds.spmode_vpn = false;
        ds.inbound_proxy_type = crate::model::INBOUND_NONE;
        let config = build_config(&proxy, &ds).unwrap();
        assert!(config["inbounds"].as_array().unwrap().is_empty());
    }

    /// Port of the GUI's DNS section: remote through the proxy, direct and
    /// local resolvers, the proxy server's domain resolved directly.
    #[test]
    fn test_dns_section() {
        let mut proxy = crate::model::ProxyEntity::new("socks");
        proxy.server_address = "proxy.example.com".into();
        let ds = crate::model::DataStore::default();
        let config = build_config(&proxy, &ds).unwrap();
        let dns = &config["dns"];
        let tags: Vec<&str> = dns["servers"]
            .as_array()
            .unwrap()
            .iter()
            .filter_map(|s| s["tag"].as_str())
            .collect();
        assert_eq!(tags, vec!["dns-remote", "dns-direct", "dns-local"]);
        assert_eq!(dns["servers"][0]["type"], "tls");
        assert_eq!(dns["servers"][0]["server"], "8.8.8.8");
        assert_eq!(dns["servers"][0]["detour"], "proxy");
        assert_eq!(dns["final"], "dns-remote");
        let direct = dns["rules"]
            .as_array()
            .unwrap()
            .iter()
            .find(|r| r["server"] == "dns-direct")
            .expect("direct rule for the proxy server");
        assert_eq!(direct["domain"], json!(["proxy.example.com"]));
        assert_eq!(config["route"]["default_domain_resolver"]["server"], "dns-direct");

        // An IP server needs no direct lookup; `dns_final_out_direct` puts
        // the direct server first and drops `final`.
        proxy.server_address = "192.0.2.1".into();
        let ds = crate::model::DataStore {
            dns_final_out_direct: true,
            ..Default::default()
        };
        let config = build_config(&proxy, &ds).unwrap();
        assert!(!config["dns"]["rules"]
            .as_array()
            .unwrap()
            .iter()
            .any(|r| r["server"] == "dns-direct"));
        assert_eq!(config["dns"]["servers"][0]["tag"], "dns-direct");
        assert!(config["dns"].get("final").is_none());

        // `use_dns_object` replaces the section with the user's object.
        let ds = crate::model::DataStore {
            use_dns_object: true,
            dns_object: Some(r#"{"servers":[{"tag":"mine","type":"local"}]}"#.into()),
            ..Default::default()
        };
        let config = build_config(&proxy, &ds).unwrap();
        assert_eq!(config["dns"]["servers"][0]["tag"], "mine");
    }

    #[test]
    fn test_build_dns_object() {
        assert_eq!(build_dns_object("localhost"), json!({ "type": "local" }));
        assert_eq!(
            build_dns_object("dhcp://auto"),
            json!({ "type": "dhcp", "interface": "" })
        );
        assert_eq!(
            build_dns_object("tls://1.1.1.1"),
            json!({ "type": "tls", "server": "1.1.1.1" })
        );
        assert_eq!(
            build_dns_object("https://dns.google/dns-query"),
            json!({ "type": "https", "server": "dns.google", "path": "dns-query" })
        );
        // The GUI separates the port with ';'.
        assert_eq!(
            build_dns_object("udp://9.9.9.9;5353"),
            json!({ "type": "udp", "server": "9.9.9.9", "server_port": 5353 })
        );
        assert_eq!(
            build_dns_object("8.8.4.4"),
            json!({ "type": "udp", "server": "8.8.4.4" })
        );
    }

    /// Named rule sets resolve to downloadable ones (the core only says
    /// "rule-set not found" at start otherwise); unknown names fail the build.
    #[test]
    fn test_named_rule_sets() {
        use crate::model::{RouteRule, RoutingChain, OUTBOUND_DIRECT};
        let mut chain = RoutingChain::new();
        chain.rules = vec![RouteRule {
            rule_set: vec!["geoip-cn".into(), "geosite-cn".into()],
            ip_cidr: vec!["198.51.100.0/24".into()],
            outbound_id: OUTBOUND_DIRECT,
            action: "route".into(),
            ..Default::default()
        }];
        let proxy = crate::model::ProxyEntity::new("socks");
        let mut ds = crate::model::DataStore {
            spmode_vpn: true,
            ..Default::default()
        };
        let config = build_config_with_route(&proxy, &ds, Some(&chain), None).unwrap();
        let sets = config["route"]["rule_set"].as_array().unwrap();
        let tags: Vec<&str> = sets.iter().filter_map(|s| s["tag"].as_str()).collect();
        assert_eq!(tags, vec!["geoip-cn", "geosite-cn"]);
        assert!(sets[0]["url"].as_str().unwrap().ends_with("/geoip/cn.srs"));
        // Direct sites resolve through the direct DNS server.
        assert!(config["dns"]["rules"]
            .as_array()
            .unwrap()
            .iter()
            .any(|r| r["server"] == "dns-direct" && r["rule_set"] == json!(["geosite-cn"])));
        // Direct IPs only bypass the TUN with `enable_tun_routing`.
        let tun = &config["inbounds"][1];
        assert!(tun.get("route_exclude_address_set").is_none());
        assert!(!tun["route_exclude_address"]
            .as_array()
            .unwrap()
            .contains(&json!("198.51.100.0/24")));
        ds.enable_tun_routing = true;
        let config = build_config_with_route(&proxy, &ds, Some(&chain), None).unwrap();
        let tun = &config["inbounds"][1];
        assert_eq!(tun["route_exclude_address_set"], json!(["geoip-cn"]));
        assert!(tun["route_exclude_address"]
            .as_array()
            .unwrap()
            .contains(&json!("198.51.100.0/24")));

        chain.rules[0].rule_set = vec!["no-such-set".into()];
        let err = build_config_with_route(&proxy, &ds, Some(&chain), None).unwrap_err();
        assert!(err.to_string().contains("no-such-set"), "{err}");
    }

    /// An unported protocol is an error, not an outbound the core rejects.
    #[test]
    fn test_unsupported_type_is_an_error() {
        let wg = crate::model::ProxyEntity::new("wireguard");
        assert!(build_outbound(&wg, false).is_err());
        assert!(build_config(&wg, &crate::model::DataStore::default()).is_err());
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
