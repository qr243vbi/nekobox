//! Subscription updaters and share-link parsing.
//!
//! Port of `src/nekobox/configs/sub/GroupUpdater.hpp` (the raw/base64 link
//! formats; SIP008/Clash/sing-box wire formats are not ported).
//!
//! Bean keys written here are the GUI's `ADD_MAP` names, so imported profiles
//! read back correctly in the Qt GUI (`pass`/`stream` on TrojanVLESSBean,
//! `id`/`aid`/`sec` on VMessBean, `username`/`password` on SocksBean, …).
//!
//! Share-link parsing supports:
//! - `vmess://` (V2Ray)
//! - `vless://` (VLESS, incl. `security=reality`)
//! - `ss://` (Shadowsocks, SIP002)
//! - `trojan://`
//! - `socks://` / `http://` / `https://`
//! - `hysteria2://` / `hy2://`, `tuic://`, `anytls://`
//! - `nekoray://` (base64-wrapped share link, the GUI's own export format)

use crate::model::ProxyEntity;
use anyhow::Context;
use std::collections::HashMap;

/// Result of parsing a share link or subscription.
#[derive(Debug, Clone)]
pub struct ParsedProxy {
    /// The parsed proxy entity
    pub entity: ProxyEntity,
    /// The original share link (if applicable)
    pub link: Option<String>,
}

/// Schemes `parse_share_link` understands.
const KNOWN_SCHEMES: &[&str] = &[
    "vmess", "vless", "ss", "trojan", "socks", "hysteria2", "hy2", "tuic", "anytls",
    "nekoray",
];

/// Parse a share link into a ProxyEntity.
pub fn parse_share_link(link: &str) -> anyhow::Result<ProxyEntity> {
    let link = link.trim();

    if let Some(rest) = link.strip_prefix("vmess://") {
        parse_vmess_link(rest)
    } else if let Some(rest) = link.strip_prefix("vless://") {
        parse_vless_link(rest)
    } else if let Some(rest) = link.strip_prefix("ss://") {
        parse_ss_link(rest)
    } else if let Some(rest) = link.strip_prefix("trojan://") {
        parse_trojan_link(rest)
    } else if let Some(rest) = link.strip_prefix("socks://") {
        parse_socks_link(rest)
    } else if let Some(rest) = link
        .strip_prefix("hysteria2://")
        .or_else(|| link.strip_prefix("hy2://"))
    {
        parse_hysteria2_link(rest)
    } else if let Some(rest) = link.strip_prefix("tuic://") {
        parse_tuic_link(rest)
    } else if let Some(rest) = link.strip_prefix("anytls://") {
        parse_anytls_link(rest)
    } else if let Some(rest) = link.strip_prefix("nekoray://") {
        let decoded = b64_decode(rest).context("invalid base64 in nekoray link")?;
        let inner = String::from_utf8(decoded).context("invalid UTF-8 in nekoray link")?;
        parse_share_link(inner.trim())
    } else if link.starts_with("http://") || link.starts_with("https://") {
        parse_http_link(link)
    } else {
        Err(anyhow::anyhow!(
            "unsupported link scheme: {}",
            link.split(':').next().unwrap_or("unknown")
        ))
    }
}

/// Remove null and empty-string entries from a JSON object
/// (other value kinds, e.g. booleans, are kept).
fn filter_empty(obj: serde_json::Value) -> serde_json::Value {
    match obj {
        serde_json::Value::Object(mut map) => {
            map.retain(|_, v| !v.is_null() && v.as_str() != Some(""));
            serde_json::Value::Object(map)
        }
        other => other,
    }
}

/// A JSON number or numeric string as i64 — share links disagree on this
/// (`"port": 443` vs `"port": "443"`; our own export writes strings).
fn json_i64(v: &serde_json::Value) -> Option<i64> {
    v.as_i64()
        .or_else(|| v.as_str().and_then(|s| s.trim().parse().ok()))
}

/// Percent-decode a URL component.
fn pdec(s: &str) -> String {
    percent_encoding::percent_decode_str(s)
        .decode_utf8_lossy()
        .into_owned()
}

/// Fragment as the profile name (percent-decoded).
fn link_name(url: &url::Url) -> String {
    url.fragment().map(pdec).unwrap_or_default()
}

/// Query string as a decoded key→value map.
fn query_map(url: &url::Url) -> HashMap<String, String> {
    url.query_pairs().map(|(k, v)| (k.into_owned(), v.into_owned())).collect()
}

/// Truthy query value: "1"/"true"/"yes".
fn qflag(map: &HashMap<String, String>, keys: &[&str]) -> bool {
    keys.iter()
        .find_map(|k| map.get(*k))
        .is_some_and(|v| v == "1" || v.eq_ignore_ascii_case("true") || v == "yes")
}

/// Build the `stream` sub-store from query parameters, using the GUI's
/// V2RayStreamSettings key names. Shared by vless/trojan-style links.
///
/// Note: the GUI maps `security=reality` to `sec: "tls"` + `pbk`/`sid` —
/// reality is detected by a non-empty `pbk` at build time
/// (`TryParseLink` in TrojanVLESSBean.cpp does `.replace("reality", "tls")`).
fn stream_from_query(q: &HashMap<String, String>) -> serde_json::Value {
    let sec = match q.get("security").map(String::as_str) {
        Some("tls") | Some("reality") | Some("xtls") => "tls".to_string(),
        _ => String::new(),
    };
    filter_empty(serde_json::json!({
        "net": q.get("type").cloned().unwrap_or_default(),
        "sec": sec,
        "sni": q.get("sni").cloned().unwrap_or_default(),
        "host": q.get("host").cloned().unwrap_or_default(),
        "path": q.get("path").cloned().unwrap_or_default(),
        "alpn": q.get("alpn").cloned().unwrap_or_default(),
        "insecure": qflag(q, &["allowInsecure", "allow_insecure"]),
        "utls": q.get("fp").cloned().unwrap_or_default(),
        "pbk": q.get("pbk").cloned().unwrap_or_default(),
        "sid": q.get("sid").cloned().unwrap_or_default(),
    }))
}

/// Parse a vmess share link (base64 JSON payload).
fn parse_vmess_link(encoded: &str) -> anyhow::Result<ProxyEntity> {
    let decoded = base64::Engine::decode(&base64::engine::general_purpose::STANDARD, encoded.trim())
        .or_else(|_| {
            base64::Engine::decode(
                &base64::engine::general_purpose::URL_SAFE_NO_PAD,
                encoded.trim(),
            )
        })
        .context("invalid base64 in vmess link")?;
    let json_str = String::from_utf8(decoded).context("invalid UTF-8 in vmess payload")?;
    let vmess: serde_json::Value = serde_json::from_str(&json_str)
        .context("invalid JSON in vmess payload")?;

    let mut entity = ProxyEntity::new("vmess");
    entity.name = vmess["ps"].as_str().unwrap_or("").to_string();
    entity.server_address = vmess["add"].as_str().unwrap_or("127.0.0.1").to_string();
    entity.server_port = json_i64(&vmess["port"]).unwrap_or(443) as i32;

    // Bean config
    let stream = filter_empty(serde_json::json!({
        "net": vmess["net"].as_str().unwrap_or("tcp"),
        "sec": if vmess["tls"].as_str().unwrap_or("") == "tls" { "tls" } else { "" },
        "sni": vmess["sni"].as_str().unwrap_or(""),
        "host": vmess["host"].as_str().unwrap_or(""),
        "path": vmess["path"].as_str().unwrap_or(""),
        "alpn": vmess["alpn"].as_str().unwrap_or(""),
    }));
    let bean = serde_json::json!({
        "id": vmess["id"].as_str().unwrap_or(""),
        "aid": json_i64(&vmess["aid"]).unwrap_or(0),
        "sec": vmess["scy"].as_str().or(vmess["sec"].as_str()).unwrap_or("auto"),
        "stream": stream,
    });

    entity.bean_cfg = Some(bean);
    Ok(entity)
}

/// Parse a vless share link.
fn parse_vless_link(url_str: &str) -> anyhow::Result<ProxyEntity> {
    let url = url::Url::parse(&format!("vless://{}", url_str))
        .context("invalid vless URL")?;

    let mut entity = ProxyEntity::new("vless");
    entity.name = link_name(&url);
    entity.server_address = url.host_str().unwrap_or("127.0.0.1").to_string();
    if let Some(port) = url.port() {
        entity.server_port = port as i32;
    }

    let q = query_map(&url);
    let mut bean = serde_json::json!({
        // GUI key is "pass" (TrojanVLESSBean), even though it is a UUID.
        "pass": pdec(url.username()),
        "stream": stream_from_query(&q),
    });
    let flow = q.get("flow").cloned().unwrap_or_default();
    if !flow.is_empty() && flow != "none" {
        bean["flow"] = flow.into();
    }
    if let Some(enc) = q.get("encryption").filter(|e| !e.is_empty()) {
        bean["enc"] = enc.clone().into();
    }
    entity.bean_cfg = Some(bean);

    Ok(entity)
}

/// Decode base64, trying URL-safe (no padding) first, then standard.
fn b64_decode(s: &str) -> anyhow::Result<Vec<u8>> {
    use base64::Engine;
    let s = s.trim();
    base64::engine::general_purpose::URL_SAFE_NO_PAD
        .decode(s)
        .or_else(|_| base64::engine::general_purpose::STANDARD.decode(s))
        .context("invalid base64")
}

/// Parse a shadowsocks share link payload (scheme already stripped).
///
/// Supported forms (SIP002 and legacy):
/// - `base64url(method:pass)@host:port/?plugin=...#tag`
/// - `method:pass@host:port#tag`
/// - `base64(method:pass@host:port#tag)` (whole payload encoded)
fn parse_ss_link(encoded: &str) -> anyhow::Result<ProxyEntity> {
    // Strip fragment (tag)
    let (body, tag) = match encoded.split_once('#') {
        Some((b, t)) => (b, t.to_string()),
        None => (encoded, String::new()),
    };
    let body = body.trim_end_matches('/');

    // Whole-payload base64 (no '@' visible outside the encoding)
    let body_owned;
    let body = if body.contains('@') {
        body
    } else {
        let decoded = b64_decode(body)?;
        body_owned = String::from_utf8(decoded)?;
        body_owned
            .split_once('#')
            .map(|(b, _)| b)
            .unwrap_or(&body_owned)
            .trim_end_matches('/')
    };

    let (userinfo, host_port) = body
        .rsplit_once('@')
        .ok_or_else(|| anyhow::anyhow!("invalid ss link format"))?;

    // userinfo is either `method:pass` (possibly percent-encoded) or base64 of it
    let userinfo_owned;
    let userinfo = if userinfo.contains(':') {
        userinfo
    } else {
        let decoded = b64_decode(userinfo)?;
        userinfo_owned = String::from_utf8(decoded)?;
        &userinfo_owned
    };
    let (method, password) = match userinfo.split_once(':') {
        Some((m, p)) => (pdec(m), pdec(p)),
        None => (userinfo.to_string(), String::new()),
    };
    // sing-box only knows the IETF name.
    let method = match method.as_str() {
        "chacha20-poly1305" => "chacha20-ietf-poly1305".to_string(),
        other => other.to_string(),
    };

    // host:port — the host part may carry a `?plugin=...` query (SIP002)
    let (host_port, plugin_query) = match host_port.split_once('?') {
        Some((h, q)) => (h, Some(q.trim_end_matches('/'))),
        None => (host_port, None),
    };
    let (host, port) = match host_port.rsplit_once(':') {
        Some((h, p)) => (
            h.trim_start_matches('[').trim_end_matches(']').to_string(),
            p.parse().unwrap_or(8388),
        ),
        None => (host_port.to_string(), 8388),
    };

    let mut bean = serde_json::json!({
        "method": method,
        "pass": password,
    });
    if let Some(q) = plugin_query {
        // plugin=obfs-local;obfs=http;obfs-host=... (percent-encoded)
        let plugin = pdec(q.strip_prefix("plugin=").unwrap_or(q));
        let mut it = plugin.splitn(2, ';');
        if let Some(name) = it.next().filter(|s| !s.is_empty()) {
            bean["plugin"] = name.into();
        }
        if let Some(opts) = it.next().filter(|s| !s.is_empty()) {
            bean["plugin_opts"] = opts.into();
        }
    }

    let mut entity = ProxyEntity::new("shadowsocks");
    entity.name = pdec(&tag);
    entity.server_address = host;
    entity.server_port = port;
    entity.bean_cfg = Some(bean);

    Ok(entity)
}

/// Parse a trojan share link.
fn parse_trojan_link(url_str: &str) -> anyhow::Result<ProxyEntity> {
    let url = url::Url::parse(&format!("trojan://{}", url_str))
        .context("invalid trojan URL")?;

    let mut entity = ProxyEntity::new("trojan");
    entity.name = link_name(&url);
    entity.server_address = url.host_str().unwrap_or("127.0.0.1").to_string();
    entity.server_port = url.port().unwrap_or(443) as i32;

    let q = query_map(&url);
    let mut stream = stream_from_query(&q);
    // Trojan is always TLS; an absent `security` parameter still means tls.
    if stream.get("sec").and_then(|v| v.as_str()).unwrap_or("").is_empty() {
        stream["sec"] = "tls".into();
    }
    entity.bean_cfg = Some(serde_json::json!({
        "pass": pdec(url.username()),
        "stream": stream,
    }));

    Ok(entity)
}

/// Parse a SOCKS share link.
fn parse_socks_link(url_str: &str) -> anyhow::Result<ProxyEntity> {
    let url = url::Url::parse(&format!("socks://{}", url_str))
        .context("invalid socks URL")?;

    let mut entity = ProxyEntity::new("socks");
    entity.name = link_name(&url);
    entity.server_address = url.host_str().unwrap_or("127.0.0.1").to_string();
    entity.server_port = url.port().unwrap_or(1080) as i32;

    let mut bean = serde_json::json!({});
    if !url.username().is_empty() {
        bean["username"] = pdec(url.username()).into();
        bean["password"] = pdec(url.password().unwrap_or("")).into();
    }
    entity.bean_cfg = Some(bean);

    Ok(entity)
}

/// Parse an HTTP(S) share link.
fn parse_http_link(link: &str) -> anyhow::Result<ProxyEntity> {
    let url = url::Url::parse(link)?;

    let mut entity = ProxyEntity::new("http");
    entity.server_address = url.host_str().unwrap_or("127.0.0.1").to_string();
    entity.server_port = url.port().unwrap_or(80) as i32;
    let name = link_name(&url);
    entity.name = if name.is_empty() {
        format!("{}:{}", entity.server_address, entity.server_port)
    } else {
        name
    };

    let mut bean = serde_json::json!({});
    if !url.username().is_empty() {
        bean["username"] = pdec(url.username()).into();
        bean["password"] = pdec(url.password().unwrap_or("")).into();
    }
    entity.bean_cfg = Some(bean);

    Ok(entity)
}

/// Hysteria2 port syntax: `mport`-style `443,500-600` in place of the port,
/// or a plain port. Returns (first_port, server_ports as "443:443"/"500:600").
fn parse_hy2_ports(s: &str) -> (Option<i32>, Vec<String>) {
    let mut first = None;
    let mut ranges = Vec::new();
    for part in s.split(',') {
        let part = part.trim();
        if part.is_empty() {
            continue;
        }
        // The link format uses '-' for ranges; the bean stores "500:600".
        let range = part.replace('-', ":");
        if first.is_none() {
            first = range.split(':').next().and_then(|p| p.parse().ok());
        }
        ranges.push(range);
    }
    (first, ranges)
}

/// Hysteria2 links put the port list in the port position
/// (`hy2://pw@host:443,500-600?...`), which the `url` crate rejects as a
/// non-numeric port. Split it out before parsing; ranges use `-` in links,
/// `:` in the bean. Returns (sanitized link payload, port, server_ports).
fn split_hy2_ports(s: &str) -> (String, Option<i32>, Vec<String>) {
    // s = "userinfo@host:portpart[/path][?query][#frag]"
    let Some(at) = s.find('@') else {
        return (s.to_string(), None, Vec::new());
    };
    let rest = &s[at + 1..];
    let end = rest.find(['/', '?', '#']).unwrap_or(rest.len());
    let host_port = &rest[..end];
    let Some(colon) = host_port.rfind(':') else {
        return (s.to_string(), None, Vec::new());
    };
    let port_part = &host_port[colon + 1..];
    let is_range_list = port_part.contains(',') || port_part.contains('-');
    if !is_range_list {
        return (s.to_string(), None, Vec::new());
    }
    let mut first = None;
    let mut ranges = Vec::new();
    for part in port_part.split(',') {
        let part = part.trim();
        if part.is_empty() {
            continue;
        }
        let range = part.replace('-', ":");
        if first.is_none() {
            first = range.split(':').next().and_then(|p| p.parse::<i32>().ok());
        }
        ranges.push(range);
    }
    let sanitized = format!(
        "{}{}:{}{}",
        &s[..at + 1],
        &host_port[..colon],
        first.unwrap_or(443),
        &rest[end..]
    );
    (sanitized, first, ranges)
}

/// Parse a hysteria2/hy2 share link.
fn parse_hysteria2_link(url_str: &str) -> anyhow::Result<ProxyEntity> {
    let (sanitized, range_port, ranges) = split_hy2_ports(url_str);
    let url = url::Url::parse(&format!("hy2://{}", sanitized))
        .context("invalid hysteria2 URL")?;

    let mut entity = ProxyEntity::new("hysteria2");
    entity.name = link_name(&url);
    entity.server_address = url.host_str().unwrap_or("127.0.0.1").to_string();

    let q = query_map(&url);
    // The port position may hold an mport list ("443,500-600"), or a plain
    // port, or an explicit mport query parameter.
    let mut bean = serde_json::json!({});
    if let Some(port) = range_port.or_else(|| url.port().map(|p| p as i32)) {
        entity.server_port = port;
    } else if let Some(mp) = q.get("mport") {
        let (first, rs) = parse_hy2_ports(mp);
        entity.server_port = first.unwrap_or(443);
        if !rs.is_empty() {
            bean["server_ports"] = rs.into();
        }
    } else {
        entity.server_port = 443;
    }
    if ranges.len() > 1 {
        bean["server_ports"] = ranges.into();
    }
    // userinfo may be "password" or "user:password" (combined back with ':')
    let password = match url.password() {
        Some(p) => format!("{}:{}", pdec(url.username()), pdec(p)),
        None => pdec(url.username()),
    };
    bean["password"] = password.into();
    if let Some(sni) = q.get("sni").filter(|s| !s.is_empty()) {
        bean["sni"] = sni.clone().into();
    }
    if qflag(&q, &["insecure", "allow_insecure"]) {
        bean["allowInsecure"] = true.into();
    }
    if q.contains_key("obfs") {
        if let Some(op) = q.get("obfs-password").filter(|s| !s.is_empty()) {
            bean["obfsPassword"] = op.clone().into();
        }
    }
    if let Some(h) = q.get("hop_interval").filter(|s| !s.is_empty()) {
        bean["hop_interval"] = h.clone().into();
    }
    entity.bean_cfg = Some(bean);
    Ok(entity)
}

/// Parse a tuic share link.
fn parse_tuic_link(url_str: &str) -> anyhow::Result<ProxyEntity> {
    let url = url::Url::parse(&format!("tuic://{}", url_str))
        .context("invalid tuic URL")?;

    let mut entity = ProxyEntity::new("tuic");
    entity.name = link_name(&url);
    entity.server_address = url.host_str().unwrap_or("127.0.0.1").to_string();
    entity.server_port = url.port().unwrap_or(443) as i32;

    let q = query_map(&url);
    entity.bean_cfg = Some(filter_empty(serde_json::json!({
        "uuid": pdec(url.username()),
        "password": pdec(url.password().unwrap_or("")),
        "congestionControl": q.get("congestion_control").cloned().unwrap_or_default(),
        "udpRelayMode": q.get("udp_relay_mode").cloned().unwrap_or_default(),
        "sni": q.get("sni").cloned().unwrap_or_default(),
        "alpn": q.get("alpn").cloned().unwrap_or_default(),
        "allowInsecure": qflag(&q, &["allow_insecure", "insecure"]),
        "disableSni": qflag(&q, &["disable_sni"]),
        "zeroRttHandshake": qflag(&q, &["zero_rtt_handshake"]),
    })));
    Ok(entity)
}

/// Parse an anytls share link (`anytls://password@host:port?sni=...&insecure=1`).
fn parse_anytls_link(url_str: &str) -> anyhow::Result<ProxyEntity> {
    let url = url::Url::parse(&format!("anytls://{}", url_str))
        .context("invalid anytls URL")?;

    let mut entity = ProxyEntity::new("anytls");
    entity.name = link_name(&url);
    entity.server_address = url.host_str().unwrap_or("127.0.0.1").to_string();
    entity.server_port = url.port().unwrap_or(443) as i32;

    let q = query_map(&url);
    entity.bean_cfg = Some(serde_json::json!({
        "password": pdec(url.username()),
        "stream": filter_empty(serde_json::json!({
            "sec": "tls",
            "sni": q.get("sni").cloned().unwrap_or_default(),
            "insecure": qflag(&q, &["insecure", "allow_insecure"]),
        })),
    }));
    Ok(entity)
}

/// Parse a subscription string (raw links, one per line, or base64 thereof).
pub fn parse_subscription(content: &str) -> anyhow::Result<Vec<ParsedProxy>> {
    parse_subscription_depth(content.trim(), 0)
}

fn parse_subscription_depth(content: &str, depth: u8) -> anyhow::Result<Vec<ParsedProxy>> {
    let urls: Vec<&str> = content
        .lines()
        .map(str::trim)
        .filter(|l| {
            KNOWN_SCHEMES
                .iter()
                .any(|s| l.starts_with(&format!("{s}://")))
                || l.starts_with("http://")
                || l.starts_with("https://")
        })
        .collect();

    if !urls.is_empty() {
        let mut out = Vec::new();
        let mut errors = 0;
        for url in urls {
            match parse_share_link(url) {
                Ok(e) => out.push(ParsedProxy {
                    entity: e,
                    link: Some(url.to_string()),
                }),
                // Skip individual bad links instead of failing the whole sub.
                Err(_) => errors += 1,
            }
        }
        if out.is_empty() && errors > 0 {
            anyhow::bail!("no parseable links in subscription");
        }
        return Ok(out);
    }

    // Try base64 decode (a whole link list encoded as one blob).
    if depth == 0 && content.len() > 10 && content.len() < 10_000_000 {
        if let Ok(decoded) = base64::Engine::decode(
            &base64::engine::general_purpose::STANDARD,
            content.trim(),
        ) {
            if let Ok(text) = String::from_utf8(decoded) {
                if text != content {
                    return parse_subscription_depth(text.trim(), depth + 1);
                }
            }
        }
    }

    Err(anyhow::anyhow!(
        "unable to parse subscription content: unknown format"
    ))
}

// ============================================================================
// Share link export
// ============================================================================

/// Export a profile as a share link. Inverse of [`parse_share_link`].
pub fn to_share_link(entity: &ProxyEntity) -> anyhow::Result<String> {
    use base64::Engine;
    let bean = entity.bean_cfg.clone().unwrap_or_else(|| serde_json::json!({}));
    let b = &bean;
    let name = entity.display_name_str();

    let b64 = |s: &str| base64::engine::general_purpose::STANDARD.encode(s);
    let b64url = |s: &str| base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(s);
    let enc = |s: &str| {
        percent_encoding::percent_encode(s.as_bytes(), percent_encoding::NON_ALPHANUMERIC)
            .to_string()
    };
    let qs = |pairs: Vec<(&str, String)>| -> String {
        pairs
            .iter()
            .map(|(k, v)| format!("{k}={v}"))
            .collect::<Vec<_>>()
            .join("&")
    };
    let stream = &b["stream"];
    let stream_query = || -> Vec<(&'static str, String)> {
        let mut q = Vec::new();
        let sec = stream["sec"].as_str().unwrap_or("");
        let pbk = stream["pbk"].as_str().unwrap_or("");
        // Reality is stored as sec=tls + pbk; export it back as such.
        if sec == "tls" && !pbk.is_empty() {
            q.push(("security", "reality".into()));
        } else if !sec.is_empty() {
            q.push(("security", sec.into()));
        }
        let net = stream["net"].as_str().unwrap_or("");
        if !net.is_empty() && net != "tcp" {
            q.push(("type", net.into()));
        }
        for (k, field) in [("sni", "sni"), ("host", "host"), ("path", "path"), ("fp", "utls"), ("pbk", "pbk"), ("sid", "sid")] {
            let v = stream[field].as_str().unwrap_or("");
            if !v.is_empty() {
                q.push((k, enc(v)));
            }
        }
        if stream["insecure"].as_bool().unwrap_or(false) {
            q.push(("allowInsecure", "1".into()));
        }
        q
    };

    match entity.r#type.as_str() {
        "shadowsocks" => {
            let method = b["method"].as_str().unwrap_or("aes-128-gcm");
            let password = b["pass"].as_str().or(b["password"].as_str()).unwrap_or("");
            let userinfo = b64url(&format!("{method}:{password}"));
            let mut link = format!(
                "ss://{}@{}:{}",
                userinfo, entity.server_address, entity.server_port
            );
            let plugin = b["plugin"].as_str().unwrap_or("");
            if !plugin.is_empty() {
                let opts = b["plugin_opts"].as_str().unwrap_or("");
                let full = if opts.is_empty() {
                    plugin.to_string()
                } else {
                    format!("{plugin};{opts}")
                };
                link.push_str(&format!("/?plugin={}", enc(&full)));
            }
            link.push_str(&format!("#{}", enc(&name)));
            Ok(link)
        }
        "vmess" => {
            let payload = serde_json::json!({
                "v": "2",
                "ps": name,
                "add": entity.server_address,
                "port": entity.server_port.to_string(),
                "id": b["id"].as_str().unwrap_or(""),
                "aid": b["aid"].as_i64().unwrap_or(0),
                "scy": b["sec"].as_str().unwrap_or("auto"),
                "net": stream["net"].as_str().unwrap_or("tcp"),
                "type": "none",
                "host": stream["host"].as_str().unwrap_or(""),
                "path": stream["path"].as_str().unwrap_or(""),
                "tls": if stream["sec"].as_str().unwrap_or("") == "tls" { "tls" } else { "" },
                "sni": stream["sni"].as_str().unwrap_or(""),
                "alpn": stream["alpn"].as_str().unwrap_or(""),
            });
            Ok(format!("vmess://{}", b64(&payload.to_string())))
        }
        "vless" | "trojan" => {
            let password = b["pass"]
                .as_str()
                .or(b["id"].as_str())
                .or(b["password"].as_str())
                .unwrap_or("");
            let mut query = stream_query();
            if entity.r#type == "vless" {
                if let Some(flow) = b["flow"].as_str() {
                    if !flow.is_empty() && flow != "none" {
                        query.push(("flow", flow.into()));
                    }
                }
                if let Some(e) = b["enc"].as_str() {
                    if !e.is_empty() {
                        query.push(("encryption", e.into()));
                    }
                }
            }
            let scheme = entity.r#type.as_str();
            let query_string = qs(query);
            Ok(format!(
                "{scheme}://{}@{}:{}{}{}{}",
                enc(password),
                entity.server_address,
                entity.server_port,
                if query_string.is_empty() { "" } else { "?" },
                query_string,
                if name.is_empty() {
                    String::new()
                } else {
                    format!("#{}", enc(&name))
                },
            ))
        }
        "hysteria2" => {
            let mut query: Vec<(&str, String)> = Vec::new();
            if let Some(obfs) = b["obfsPassword"].as_str().filter(|s| !s.is_empty()) {
                query.push(("obfs", "salamander".into()));
                query.push(("obfs-password", enc(obfs)));
            }
            if b["allowInsecure"].as_bool().unwrap_or(false) {
                query.push(("insecure", "1".into()));
            }
            for (k, field) in [("sni", "sni"), ("hop_interval", "hop_interval")] {
                if let Some(v) = b[field].as_str().filter(|s| !s.is_empty()) {
                    query.push((k, enc(v)));
                }
            }
            // Port ranges go into the port position as "443,500-600".
            let port = match b["server_ports"].as_array() {
                Some(ports) if !ports.is_empty() => ports
                    .iter()
                    .filter_map(|p| p.as_str().map(|s| s.replace(':', "-")))
                    .collect::<Vec<_>>()
                    .join(","),
                _ => entity.server_port.to_string(),
            };
            let password = b["password"].as_str().unwrap_or("");
            Ok(format!(
                "hy2://{}@{}:{}{}{}{}",
                enc(password),
                entity.server_address,
                port,
                if query.is_empty() { "" } else { "?" },
                qs(query),
                if name.is_empty() {
                    String::new()
                } else {
                    format!("#{}", enc(&name))
                },
            ))
        }
        "tuic" => {
            let mut query: Vec<(&str, String)> = Vec::new();
            for (k, field) in [
                ("congestion_control", "congestionControl"),
                ("udp_relay_mode", "udpRelayMode"),
                ("sni", "sni"),
                ("alpn", "alpn"),
            ] {
                if let Some(v) = b[field].as_str().filter(|s| !s.is_empty()) {
                    query.push((k, enc(v)));
                }
            }
            if b["allowInsecure"].as_bool().unwrap_or(false) {
                query.push(("allow_insecure", "1".into()));
            }
            if b["disableSni"].as_bool().unwrap_or(false) {
                query.push(("disable_sni", "1".into()));
            }
            Ok(format!(
                "tuic://{}:{}@{}:{}{}{}{}",
                enc(b["uuid"].as_str().unwrap_or("")),
                enc(b["password"].as_str().unwrap_or("")),
                entity.server_address,
                entity.server_port,
                if query.is_empty() { "" } else { "?" },
                qs(query),
                if name.is_empty() {
                    String::new()
                } else {
                    format!("#{}", enc(&name))
                },
            ))
        }
        "anytls" => {
            let mut query: Vec<(&str, String)> = Vec::new();
            if let Some(sni) = stream["sni"].as_str().filter(|s| !s.is_empty()) {
                query.push(("sni", enc(sni)));
            }
            if stream["insecure"].as_bool().unwrap_or(false) {
                query.push(("insecure", "1".into()));
            }
            Ok(format!(
                "anytls://{}@{}:{}{}{}{}",
                enc(b["password"].as_str().unwrap_or("")),
                entity.server_address,
                entity.server_port,
                if query.is_empty() { "" } else { "?" },
                qs(query),
                if name.is_empty() {
                    String::new()
                } else {
                    format!("#{}", enc(&name))
                },
            ))
        }
        other => Err(anyhow::anyhow!("share link export not supported for {other}")),
    }
}

// ============================================================================
// Subscription fetching
// ============================================================================

/// Fetch a subscription URL and return its body (blocking — call from a
/// worker thread).
pub fn fetch_subscription(url: &str, user_agent: Option<&str>) -> anyhow::Result<String> {
    let mut req = reqwest::blocking::Client::builder()
        .timeout(std::time::Duration::from_secs(30))
        .build()?
        .get(url);
    if let Some(ua) = user_agent {
        if !ua.is_empty() {
            req = req.header(reqwest::header::USER_AGENT, ua);
        }
    }
    let resp = req.send()?;
    if !resp.status().is_success() {
        anyhow::bail!("subscription fetch failed: HTTP {}", resp.status());
    }
    Ok(resp.text()?)
}

/// Fetch and parse a subscription into proxy entities.
pub fn update_subscription(url: &str, user_agent: Option<&str>) -> anyhow::Result<Vec<ParsedProxy>> {
    let body = fetch_subscription(url, user_agent)?;
    parse_subscription(&body)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_ss_link_basic() {
        // ss://aes-128-gcm:testpass@example.com:443/#Test
        let link = "ss://YWVzLTEyOC1nY206dGVzdHBhc3NAZXhhbXBsZS5jb206NDQzLw==#Test";
        let result = parse_ss_link(&link[5..]);
        assert!(result.is_ok());
        let entity = result.unwrap();
        assert_eq!(entity.r#type, "shadowsocks");
        assert_eq!(entity.server_address, "example.com");
        assert_eq!(entity.server_port, 443);
        assert_eq!(entity.name, "Test");
    }

    #[test]
    fn test_share_link_roundtrip() {
        let link = "ss://YWVzLTEyOC1nY206dGVzdHBhc3NAZXhhbXBsZS5jb206NDQzLw==#Test";
        let e = parse_share_link(link).unwrap();
        let exported = to_share_link(&e).unwrap();
        let e2 = parse_share_link(&exported).unwrap();
        assert_eq!(e2.server_address, "example.com");
        assert_eq!(e2.server_port, 443);
        assert_eq!(e2.name, "Test");
        assert_eq!(
            e2.bean_cfg.unwrap()["pass"],
            serde_json::json!("testpass")
        );
    }

    /// The export writes the port as a string; the parser must accept that
    /// (and the numeric form other generators produce).
    #[test]
    fn test_vmess_port_string_roundtrip() {
        let mut e = ProxyEntity::new("vmess");
        e.server_address = "vm.example.com".into();
        e.server_port = 38585;
        e.name = "vm".into();
        e.bean_cfg = Some(serde_json::json!({
            "id": "00000000-0000-4000-8000-000000000000",
            "stream": {"net": "ws", "sec": "tls", "sni": "s.example.com"},
        }));
        let link = to_share_link(&e).unwrap();
        let e2 = parse_share_link(&link).unwrap();
        assert_eq!(e2.server_port, 38585);
        assert_eq!(e2.server_address, "vm.example.com");
        assert_eq!(e2.bean_cfg.as_ref().unwrap()["stream"]["sec"], "tls");
        assert_eq!(
            e2.bean_cfg.as_ref().unwrap()["stream"]["sni"],
            "s.example.com"
        );
    }

    /// Trojan links must land in the GUI's keys (`pass`, `stream`), not
    /// ad-hoc ones — otherwise the GUI shows an empty password and the TUI's
    /// own config builder drops the transport.
    #[test]
    fn test_trojan_link_uses_gui_keys() {
        let link = "trojan://pw@tr.example.com:443?type=ws&path=%2Fws&host=h.example.com&sni=s.example.com#tr";
        let e = parse_share_link(link).unwrap();
        let bean = e.bean_cfg.as_ref().unwrap();
        assert_eq!(bean["pass"], "pw");
        assert_eq!(bean["stream"]["net"], "ws");
        assert_eq!(bean["stream"]["path"], "/ws");
        assert_eq!(bean["stream"]["sec"], "tls");
        assert_eq!(bean["stream"]["sni"], "s.example.com");

        // Roundtrip through the export path.
        let e2 = parse_share_link(&to_share_link(&e).unwrap()).unwrap();
        assert_eq!(e2.bean_cfg.as_ref().unwrap()["pass"], "pw");
        assert_eq!(e2.bean_cfg.as_ref().unwrap()["stream"]["path"], "/ws");
    }

    #[test]
    fn test_vless_reality_link() {
        let link = "vless://00000000-0000-4000-8000-000000000000@vl.example.com:443?security=reality&pbk=PUBKEY&sid=ab&fp=chrome&flow=xtls-rprx-vision&sni=www.microsoft.com&type=tcp#vl";
        let e = parse_share_link(link).unwrap();
        let bean = e.bean_cfg.as_ref().unwrap();
        assert_eq!(bean["pass"], "00000000-0000-4000-8000-000000000000");
        assert_eq!(bean["flow"], "xtls-rprx-vision");
        // The GUI convention: reality is sec=tls + pbk/sid/utls.
        assert_eq!(bean["stream"]["sec"], "tls");
        assert_eq!(bean["stream"]["pbk"], "PUBKEY");
        assert_eq!(bean["stream"]["sid"], "ab");
        assert_eq!(bean["stream"]["utls"], "chrome");

        // And the export turns it back into security=reality.
        let e2 = parse_share_link(&to_share_link(&e).unwrap()).unwrap();
        let bean2 = e2.bean_cfg.as_ref().unwrap();
        assert_eq!(bean2["stream"]["sec"], "tls");
        assert_eq!(bean2["stream"]["pbk"], "PUBKEY");
        assert_eq!(bean2["flow"], "xtls-rprx-vision");
        assert!(to_share_link(&e).unwrap().contains("security=reality"));
    }

    #[test]
    fn test_socks_http_auth() {
        let e = parse_share_link("socks://user:p%40ss@so.example.com:1080#s").unwrap();
        assert_eq!(e.bean_cfg.as_ref().unwrap()["username"], "user");
        assert_eq!(e.bean_cfg.as_ref().unwrap()["password"], "p@ss");

        let e = parse_share_link("http://user:pw@h.example.com:8080").unwrap();
        assert_eq!(e.bean_cfg.as_ref().unwrap()["username"], "user");
        assert_eq!(e.bean_cfg.as_ref().unwrap()["password"], "pw");
        assert_eq!(e.name, "h.example.com:8080");
    }

    #[test]
    fn test_hysteria2_link() {
        let link = "hy2://p%40ss@hy2.example.com:443,500-600?sni=hy.example.com&insecure=1&obfs=salamander&obfs-password=obfspw&hop_interval=30s#hy";
        let e = parse_share_link(link).unwrap();
        assert_eq!(e.r#type, "hysteria2");
        assert_eq!(e.server_port, 443);
        assert_eq!(e.name, "hy");
        let bean = e.bean_cfg.as_ref().unwrap();
        assert_eq!(bean["password"], "p@ss");
        assert_eq!(bean["sni"], "hy.example.com");
        assert_eq!(bean["allowInsecure"], true);
        assert_eq!(bean["obfsPassword"], "obfspw");
        assert_eq!(bean["hop_interval"], "30s");
        assert_eq!(bean["server_ports"], serde_json::json!(["443", "500:600"]));

        // Roundtrip.
        let e2 = parse_share_link(&to_share_link(&e).unwrap()).unwrap();
        let bean2 = e2.bean_cfg.as_ref().unwrap();
        assert_eq!(bean2["password"], "p@ss");
        assert_eq!(bean2["obfsPassword"], "obfspw");
        assert_eq!(bean2["server_ports"], serde_json::json!(["443", "500:600"]));
    }

    #[test]
    fn test_tuic_link() {
        let link = "tuic://00000000-0000-4000-8000-000000000000:pw@tuic.example.com:443?congestion_control=bbr&udp_relay_mode=quic&sni=t.example.com&alpn=h3&allow_insecure=1#tc";
        let e = parse_share_link(link).unwrap();
        let bean = e.bean_cfg.as_ref().unwrap();
        assert_eq!(bean["uuid"], "00000000-0000-4000-8000-000000000000");
        assert_eq!(bean["password"], "pw");
        assert_eq!(bean["congestionControl"], "bbr");
        assert_eq!(bean["udpRelayMode"], "quic");
        assert_eq!(bean["allowInsecure"], true);
    }

    #[test]
    fn test_anytls_link() {
        let link = "anytls://pw@at.example.com:8443?sni=a.example.com#at";
        let e = parse_share_link(link).unwrap();
        assert_eq!(e.r#type, "anytls");
        assert_eq!(e.server_port, 8443);
        let bean = e.bean_cfg.as_ref().unwrap();
        assert_eq!(bean["password"], "pw");
        assert_eq!(bean["stream"]["sec"], "tls");
        assert_eq!(bean["stream"]["sni"], "a.example.com");
    }

    #[test]
    fn test_nekoray_link() {
        let inner = to_share_link(&parse_share_link(
            "ss://YWVzLTEyOC1nY206dGVzdHBhc3NAZXhhbXBsZS5jb206NDQzLw==#Test",
        ).unwrap()).unwrap();
        let wrapped = format!(
            "nekoray://{}",
            base64::Engine::encode(
                &base64::engine::general_purpose::URL_SAFE_NO_PAD,
                &inner
            )
        );
        let e = parse_share_link(&wrapped).unwrap();
        assert_eq!(e.server_address, "example.com");
    }

    #[test]
    fn test_subscription_skips_bad_links() {
        let sub = "ss://YWVzLTEyOC1nY206dGVzdHBhc3NAZXhhbXBsZS5jb206NDQzLw==#Test\nnot a link\nvmess://%%%";
        let parsed = parse_subscription(sub).unwrap();
        assert_eq!(parsed.len(), 1);
        assert_eq!(parsed[0].entity.server_address, "example.com");
    }

    #[test]
    fn test_subscription_base64_with_modern_schemes() {
        let body = "hy2://pw@hy.example.com:443#hy\ntuic://u:p@tc.example.com:443\n";
        let encoded = base64::Engine::encode(
            &base64::engine::general_purpose::STANDARD,
            body,
        );
        let parsed = parse_subscription(&encoded).unwrap();
        assert_eq!(parsed.len(), 2);
        assert_eq!(parsed[0].entity.r#type, "hysteria2");
        assert_eq!(parsed[1].entity.r#type, "tuic");
    }

    #[test]
    fn test_format_bytes() {
        use crate::model::traffic::format_bytes;
        assert_eq!(format_bytes(0, false), "0 B");
        assert_eq!(format_bytes(1024, false), "1.0 KB");
    }
}
