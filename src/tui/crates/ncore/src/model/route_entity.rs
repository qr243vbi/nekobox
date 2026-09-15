//! `RouteEntity.h` — routing chains and rules.
//!
//! Mirrors `RouteRule` and `RoutingChain` from
//! `src/nekobox/dataStore/RouteEntity.h`.

use super::config_item::JsonStoreBase;
use serde::{Deserialize, Serialize};
use serde_json::{json, Map, Value};

// ============================================================================
// RouteRule
// ============================================================================

/// A single routing rule.
///
/// Mirrors `RouteRule` from `src/nekobox/dataStore/RouteEntity.h`.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RouteRule {
    #[serde(default)]
    pub id: i32,

    /// Rule name (user-assigned)
    #[serde(default)]
    pub name: String,

    /// Rule type: 0=custom, 1=simple address, 2=process name, 3=process path
    #[serde(default)]
    pub r#type: i32,

    /// Action: "direct", "block", "proxy"
    #[serde(default)]
    pub simple_action: i32,

    /// IP version: "4", "6", "46", "64"
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub ip_version: Option<String>,

    /// Network: "tcp", "udp", "tcp,udp"
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub network: Option<String>,

    /// Protocol list: "http", "tls", "quic", "dns", "bittorrent"
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub protocol: Option<String>,

    /// Inbound tags
    #[serde(default)]
    pub inbound: Vec<String>,

    /// Domain list
    #[serde(default)]
    pub domain: Vec<String>,

    /// Domain suffix list
    #[serde(default)]
    pub domain_suffix: Vec<String>,

    /// Domain keyword list
    #[serde(default)]
    pub domain_keyword: Vec<String>,

    /// Domain regex list
    #[serde(default)]
    pub domain_regex: Vec<String>,

    /// Source IP CIDR list
    #[serde(default)]
    pub source_ip_cidr: Vec<String>,

    /// Source IP is private
    #[serde(default)]
    pub source_ip_is_private: bool,

    /// IP CIDR list
    #[serde(default)]
    pub ip_cidr: Vec<String>,

    /// IP is private
    #[serde(default)]
    pub ip_is_private: bool,

    /// Source port list
    #[serde(default)]
    pub source_port: Vec<String>,

    /// Source port range list
    #[serde(default)]
    pub source_port_range: Vec<String>,

    /// Port list
    #[serde(default)]
    pub port: Vec<String>,

    /// Port range list
    #[serde(default)]
    pub port_range: Vec<String>,

    /// Process name list
    #[serde(default)]
    pub process_name: Vec<String>,

    /// Process path list
    #[serde(default)]
    pub process_path: Vec<String>,

    /// Process path regex list
    #[serde(default)]
    pub process_path_regex: Vec<String>,

    /// Rule set list (file paths)
    #[serde(default)]
    pub rule_set: Vec<String>,

    /// Invert match
    #[serde(default)]
    pub invert: bool,

    /// Outbound ID: -1=proxy, -2=direct, -3=block, -4=dns_out
    #[serde(default = "default_outbound_id")]
    pub outbound_id: i32,

    /// Action type (sing-box 1.11+): "route", "reject", "hijack-dns"
    #[serde(default = "default_action")]
    pub action: String,

    /// Reject method: "default", "drop"
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub reject_method: Option<String>,

    /// Don't drop
    #[serde(default)]
    pub no_drop: bool,

    /// Override address (for sniff options)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub override_address: Option<String>,

    /// Override port (for route options)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub override_port: Option<String>,

    /// Sniffers list
    #[serde(default)]
    pub sniffers: Vec<String>,

    /// Sniff override dest
    #[serde(default)]
    pub sniff_override_dest: bool,

    /// Resolve strategy: "ipv4_only", "ipv6_only", "prefer_ipv4", "prefer_ipv6"
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub strategy: Option<String>,
}

fn default_outbound_id() -> i32 {
    -2 // direct
}

fn default_action() -> String {
    "route".into()
}

impl Default for RouteRule {
    fn default() -> Self {
        Self {
            id: 0,
            name: String::new(),
            r#type: 0,
            simple_action: 0,
            ip_version: None,
            network: None,
            protocol: None,
            inbound: Vec::new(),
            domain: Vec::new(),
            domain_suffix: Vec::new(),
            domain_keyword: Vec::new(),
            domain_regex: Vec::new(),
            source_ip_cidr: Vec::new(),
            source_ip_is_private: false,
            ip_cidr: Vec::new(),
            ip_is_private: false,
            source_port: Vec::new(),
            source_port_range: Vec::new(),
            port: Vec::new(),
            port_range: Vec::new(),
            process_name: Vec::new(),
            process_path: Vec::new(),
            process_path_regex: Vec::new(),
            rule_set: Vec::new(),
            invert: false,
            outbound_id: default_outbound_id(),
            action: default_action(),
            reject_method: None,
            no_drop: false,
            override_address: None,
            override_port: None,
            sniffers: Vec::new(),
            sniff_override_dest: false,
            strategy: None,
        }
    }
}

/// Outbound IDs with a fixed meaning (mirrors `proxyID`/`directID`/`blockID`
/// in `ConfigBuilder.cpp`).
pub const OUTBOUND_PROXY: i32 = -1;
pub const OUTBOUND_DIRECT: i32 = -2;
pub const OUTBOUND_BLOCK: i32 = -3;
pub const OUTBOUND_DNS: i32 = -4;

/// The `simple_action` values used by the simple-rule editor.
pub const SIMPLE_ACTION_DIRECT: i32 = 0;
pub const SIMPLE_ACTION_PROXY: i32 = 1;
pub const SIMPLE_ACTION_BLOCK: i32 = 2;

impl RouteRule {
    /// Create a new empty rule.
    pub fn new() -> Self {
        Self::default()
    }

    /// Render this rule as a sing-box `route.rules[]` entry.
    ///
    /// Port of `RouteRule::get_rule_json` (`RouteEntity.cpp`) for the
    /// non-export, non-view path: `outbound_map` resolves an outbound ID to
    /// its generated tag, and unmapped IDs fall back to the raw number the way
    /// the C++ does.
    pub fn to_rule_json(&self, outbound_map: &std::collections::HashMap<i32, String>) -> Value {
        let mut obj = Map::new();

        let non_empty = |l: &Vec<String>| -> bool { l.iter().any(|s| !s.trim().is_empty()) };
        let arr = |l: &Vec<String>| -> Value {
            l.iter()
                .filter(|s| !s.trim().is_empty())
                .cloned()
                .collect::<Vec<_>>()
                .into()
        };
        // `port`/`source_port` are numeric arrays in sing-box.
        let num_arr = |l: &Vec<String>| -> Value {
            l.iter()
                .filter_map(|s| s.trim().parse::<i64>().ok())
                .collect::<Vec<_>>()
                .into()
        };

        if let Some(v) = self.ip_version.as_deref().filter(|s| !s.is_empty()) {
            if let Ok(n) = v.parse::<i64>() {
                obj.insert("ip_version".into(), n.into());
            }
        }
        for (key, val) in [
            ("network", &self.network),
            ("protocol", &self.protocol),
        ] {
            if let Some(v) = val.as_deref().filter(|s| !s.is_empty()) {
                obj.insert(key.into(), v.into());
            }
        }
        for (key, list) in [
            ("inbound", &self.inbound),
            ("domain", &self.domain),
            ("domain_suffix", &self.domain_suffix),
            ("domain_keyword", &self.domain_keyword),
            ("domain_regex", &self.domain_regex),
            ("source_ip_cidr", &self.source_ip_cidr),
            ("ip_cidr", &self.ip_cidr),
            ("source_port_range", &self.source_port_range),
            ("port_range", &self.port_range),
            ("process_name", &self.process_name),
            ("process_path", &self.process_path),
            ("process_path_regex", &self.process_path_regex),
        ] {
            if non_empty(list) {
                obj.insert(key.into(), arr(list));
            }
        }
        for (key, list) in [("source_port", &self.source_port), ("port", &self.port)] {
            if non_empty(list) {
                obj.insert(key.into(), num_arr(list));
            }
        }
        if non_empty(&self.rule_set) {
            obj.insert(
                "rule_set".into(),
                self.rule_set
                    .iter()
                    .filter(|s| !s.trim().is_empty())
                    .map(|s| rule_set_tag(s))
                    .collect::<Vec<_>>()
                    .into(),
            );
        }
        if self.source_ip_is_private {
            obj.insert("source_ip_is_private".into(), true.into());
        }
        if self.ip_is_private {
            obj.insert("ip_is_private".into(), true.into());
        }
        if self.invert {
            obj.insert("invert".into(), true.into());
        }

        // The stored action is "route" even for block/dns outbounds; the
        // effective action comes from the outbound ID.
        let mut action = self.action.clone();
        if action == "route" {
            if self.outbound_id == OUTBOUND_BLOCK {
                action = "reject".into();
            } else if self.outbound_id == OUTBOUND_DNS {
                action = "hijack-dns".into();
            }
        }
        obj.insert("action".into(), action.clone().into());

        match action.as_str() {
            "reject" => {
                if let Some(m) = self.reject_method.as_deref().filter(|s| !s.is_empty()) {
                    obj.insert("method".into(), m.into());
                }
                if self.no_drop {
                    obj.insert("no_drop".into(), true.into());
                }
            }
            "route" | "route-options" => {
                if let Some(a) = self.override_address.as_deref().filter(|s| !s.is_empty()) {
                    obj.insert("override_address".into(), a.into());
                }
                if let Some(p) = self
                    .override_port
                    .as_deref()
                    .and_then(|s| s.parse::<i64>().ok())
                    .filter(|p| *p > 0)
                {
                    obj.insert("override_port".into(), p.into());
                }
                if action == "route" {
                    match outbound_map.get(&self.outbound_id) {
                        Some(tag) => obj.insert("outbound".into(), tag.clone().into()),
                        None => obj.insert("outbound".into(), self.outbound_id.into()),
                    };
                }
            }
            "sniff" => {
                if self.sniff_override_dest {
                    obj.insert("override_destination".into(), true.into());
                }
            }
            "resolve" => {
                if let Some(s) = self.strategy.as_deref().filter(|s| !s.is_empty()) {
                    obj.insert("strategy".into(), s.into());
                }
            }
            _ => {}
        }

        Value::Object(obj)
    }
}

/// Tag a rule-set reference gets in the generated config.
///
/// Port of `get_rule_set_name_1`: URL-ish entries ending in `.srs`/`.json`
/// become `<filename with dots replaced>-<hash>`, everything else is used
/// verbatim. The hash only has to be stable within one generated config, so
/// it need not match Qt's `qHash`.
pub fn rule_set_tag(rule_set: &str) -> String {
    match rule_set_file_name(rule_set) {
        Some(file_name) => format!("{}-{}", file_name.replace('.', "-"), str_hash(rule_set)),
        None => rule_set.to_string(),
    }
}

/// The `.srs`/`.json` file name a rule-set URL points at, if it is a URL.
fn rule_set_file_name(rule_set: &str) -> Option<String> {
    let url = url::Url::parse(rule_set).ok()?;
    let name = url.path_segments()?.next_back()?.to_string();
    if name.ends_with(".srs") || name.ends_with(".json") {
        Some(name)
    } else {
        None
    }
}

/// FNV-1a — a stable, dependency-free stand-in for Qt's `qHash`.
fn str_hash(s: &str) -> u32 {
    let mut h: u32 = 0x811c9dc5;
    for b in s.as_bytes() {
        h ^= *b as u32;
        h = h.wrapping_mul(0x01000193);
    }
    h
}

/// Build the `route.rule_set[]` entry for a rule-set reference, or `None` when
/// it is not a downloadable rule set. Port of `get_rule_set_json`.
pub fn rule_set_json(rule_set: &str) -> Option<Value> {
    let file_name = rule_set_file_name(rule_set)?;
    let format = if file_name.ends_with(".srs") {
        "binary"
    } else {
        "source"
    };
    Some(json!({
        "type": "remote",
        "format": format,
        "tag": rule_set_tag(rule_set),
        "url": rule_set,
    }))
}

/// The rule set the GUI injects when `adblock_enable` is on.
pub const ADBLOCK_RULE_SET: &str = "https://raw.githubusercontent.com/217heidai/adblockfilters/main/rules/adblocksingbox.srs";
pub const ADBLOCK_TAG: &str = "nekobox-adblocksingbox";

impl RouteRule {
    /// Check if this rule is empty (no matching criteria).
    pub fn is_empty(&self) -> bool {
        self.domain.is_empty()
            && self.domain_suffix.is_empty()
            && self.domain_keyword.is_empty()
            && self.domain_regex.is_empty()
            && self.ip_cidr.is_empty()
            && self.source_ip_cidr.is_empty()
            && self.port.is_empty()
            && self.port_range.is_empty()
            && self.source_port.is_empty()
            && self.source_port_range.is_empty()
            && self.process_name.is_empty()
            && self.process_path.is_empty()
            && self.process_path_regex.is_empty()
            && self.protocol.as_deref() == Some("")
            && self.rule_set.is_empty()
    }
}

/// A chain of routing rules.
///
/// Mirrors `RoutingChain` from `src/nekobox/dataStore/RouteEntity.h`.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct RoutingChain {
    #[serde(flatten)]
    pub base: JsonStoreBase,

    /// Chain name
    #[serde(default)]
    pub chain_name: String,

    /// URL to update this chain from (optional)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub update_url: Option<String>,

    /// Skip automatic updates
    #[serde(default)]
    pub skip_update: bool,

    /// Ordered list of rules
    #[serde(default)]
    pub rules: Vec<RouteRule>,

    /// Default outbound for unmatched traffic: -1=proxy, -2=direct, -3=block
    #[serde(default = "default_outbound_id")]
    pub default_outbound_id: i32,
}

impl Default for RoutingChain {
    fn default() -> Self {
        Self {
            base: JsonStoreBase::new(),
            chain_name: "Default".into(),
            update_url: None,
            skip_update: false,
            rules: Vec::new(),
            default_outbound_id: -1, // proxy
        }
    }
}

impl RoutingChain {
    /// Create a new empty routing chain.
    pub fn new() -> Self {
        Self::default()
    }

    /// Render all rules as a sing-box `route.rules` array.
    ///
    /// Port of `RoutingChain::get_route_rules`. When `adblock` is set the
    /// adblock reject rule is inserted before the first `route` rule, matching
    /// the C++ ordering.
    pub fn to_route_rules(
        &self,
        outbound_map: &std::collections::HashMap<i32, String>,
        adblock: bool,
    ) -> Vec<Value> {
        let adblock_rule = || json!({ "action": "reject", "rule_set": [ADBLOCK_TAG] });
        let mut out = Vec::new();
        let mut added_adblock = false;
        for rule in &self.rules {
            let json = rule.to_rule_json(outbound_map);
            if !added_adblock && adblock && json["action"] == "route" {
                out.push(adblock_rule());
                added_adblock = true;
            }
            out.push(json);
        }
        if !added_adblock && adblock {
            out.push(adblock_rule());
        }
        out
    }

    /// Every rule-set referenced by this chain, in first-seen order.
    pub fn used_rule_sets(&self) -> Vec<String> {
        let mut seen = std::collections::HashSet::new();
        let mut out = Vec::new();
        for rule in &self.rules {
            for rs in &rule.rule_set {
                let rs = rs.trim();
                if !rs.is_empty() && seen.insert(rs.to_string()) {
                    out.push(rs.to_string());
                }
            }
        }
        out
    }

    /// Outbound IDs referenced by this chain that point at a user profile
    /// (i.e. are not one of the built-in negative IDs).
    pub fn used_profile_outbounds(&self) -> Vec<i32> {
        let mut seen = std::collections::HashSet::new();
        let mut out = Vec::new();
        for rule in &self.rules {
            if rule.outbound_id >= 0 && seen.insert(rule.outbound_id) {
                out.push(rule.outbound_id);
            }
        }
        out
    }

    /// Add a domain entry to the simple rule matching `action`, creating the
    /// rule if the chain has none yet.
    ///
    /// Port of `LogRoute::addDomainToRoute`. `match_type` is one of `domain`,
    /// `keyword` or `suffix`. Returns `true` if the chain was modified
    /// (`false` means the entry was already present).
    pub fn add_domain_rule(&mut self, domain: &str, action: i32, match_type: &str) -> bool {
        let domain = domain.trim().to_lowercase();
        if domain.is_empty() {
            return false;
        }
        let outbound_id = match action {
            SIMPLE_ACTION_PROXY => OUTBOUND_PROXY,
            SIMPLE_ACTION_BLOCK => OUTBOUND_BLOCK,
            _ => OUTBOUND_DIRECT,
        };

        let rule = match self
            .rules
            .iter_mut()
            .position(|r| r.r#type == 1 && r.simple_action == action)
        {
            Some(idx) => &mut self.rules[idx],
            None => {
                let name = match action {
                    SIMPLE_ACTION_PROXY => "Proxy",
                    SIMPLE_ACTION_BLOCK => "Block",
                    _ => "Direct",
                };
                self.rules.push(RouteRule {
                    name: name.into(),
                    r#type: 1,
                    simple_action: action,
                    outbound_id,
                    action: "route".into(),
                    ..Default::default()
                });
                self.rules.last_mut().expect("just pushed")
            }
        };

        let list = match match_type {
            "domain" => &mut rule.domain,
            "keyword" => &mut rule.domain_keyword,
            _ => &mut rule.domain_suffix,
        };
        if list.iter().any(|d| d.trim().to_lowercase() == domain) {
            return false;
        }
        list.push(domain);
        true
    }

    /// Get the default "direct" routing chain.
    pub fn get_default_chain() -> Self {
        Self {
            rules: vec![
                RouteRule {
                    name: "geoip_private".into(),
                    ip_cidr: vec!["geoip:private".into()],
                    ip_is_private: true,
                    outbound_id: -2,
                    action: "route".into(),
                    ..Default::default()
                },
                RouteRule {
                    name: "geosite_private".into(),
                    domain: vec!["geosite:private".into()],
                    outbound_id: -2,
                    action: "route".into(),
                    ..Default::default()
                },
                RouteRule {
                    name: "geoip_cn".into(),
                    ip_cidr: vec!["geoip:cn".into()],
                    outbound_id: -2,
                    action: "route".into(),
                    ..Default::default()
                },
                RouteRule {
                    name: "geosite_cn".into(),
                    domain: vec!["geosite:cn".into()],
                    outbound_id: -2,
                    action: "route".into(),
                    ..Default::default()
                },
                RouteRule {
                    name: "bypass".into(),
                    outbound_id: -2,
                    action: "route".into(),
                    ..Default::default()
                },
                RouteRule {
                    name: "block".into(),
                    outbound_id: -3,
                    action: "route".into(),
                    ..Default::default()
                },
            ],
            default_outbound_id: -1, // proxy
            ..Default::default()
        }
    }
}
