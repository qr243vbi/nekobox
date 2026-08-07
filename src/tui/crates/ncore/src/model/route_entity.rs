//! `RouteEntity.h` — routing chains and rules.
//!
//! Mirrors `RouteRule` and `RoutingChain` from
//! `src/nekobox/dataStore/RouteEntity.h`.

use super::config_item::JsonStoreBase;
use serde::{Deserialize, Serialize};

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

impl RouteRule {
    /// Create a new empty rule.
    pub fn new() -> Self {
        Self::default()
    }

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
