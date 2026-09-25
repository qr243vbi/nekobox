//! `ProxyEntity.hpp` — represents a single proxy entry (VMess, Shadowsocks, etc.)
//!
//! The C++ type is a `JsonStore` with:
//! - Top-level fields: `type`, `name`, `serverAddress`, `serverPort`, `id`, `gid`, `latencyInt`, `latencyOrder`, `is_working`, `bean_cfg`
//! - A nested bean config that varies by protocol type
//!
//! The `bean_cfg` is a JSON object whose keys are protocol-specific (see bean
//! headers in `src/nekobox/configs/proxy/`). The `type` field determines the
//! protocol.

use super::config_item::JsonStoreBase;
use serde::{Deserialize, Serialize};

/// A proxy entity — a single proxy entry in a group.
///
/// Mirrors `ProxyEntity` from `src/nekobox/dataStore/ProxyEntity.hpp`.
/// The `type` field determines the protocol (vmess, shadowsocks, etc.)
/// and the bean config contains the protocol-specific settings.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ProxyEntity {
    #[serde(flatten)]
    pub base: JsonStoreBase,

    /// Protocol type: "vmess", "shadowsocks", "trojan", "socks", "http", "vless",
    /// "hysteria", "hysteria2", "tuic", "wireguard", "anytls", "shadowtls",
    /// "naive", "mieru", "juicity", "trusttunnel", "ssh", "tor", "tailscale",
    /// "custom", "extracore", "chain"
    pub r#type: String,

    /// Display type (e.g. "VMess", "Shadowsocks")
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub display_type: Option<String>,

    /// Display name (may differ from `name`)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub display_name: Option<String>,

    /// User-assigned name
    #[serde(default)]
    pub name: String,

    /// Server hostname or IP
    #[serde(default = "default_server")]
    pub server_address: String,

    /// Server port
    #[serde(default = "default_port")]
    pub server_port: i32,

    /// Unique ID (-1 = not yet assigned)
    #[serde(default)]
    pub id: i32,

    /// Group ID (0 = no group)
    #[serde(default)]
    pub gid: i32,

    /// Latency in ms (0 = not tested)
    #[serde(default)]
    pub latency_int: i32,

    /// Latency sort order
    #[serde(default)]
    pub latency_order: i32,

    /// Whether this proxy is currently working
    #[serde(default)]
    pub is_working: bool,

    /// Download speed display string
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub dl_speed: Option<String>,

    /// Upload speed display string
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub ul_speed: Option<String>,

    /// Test country code
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub test_country: Option<String>,

    /// Protocol-specific bean config (serialized as JSON object)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub bean_cfg: Option<serde_json::Value>,

    /// Last auto-test timestamp (Unix epoch ms)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub last_auto_test_time: Option<i64>,

    /// Full test report string
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub full_test_report: Option<String>,

    /// Cumulative download bytes (from the `traffic` sub-store)
    #[serde(default)]
    pub traffic_dl: i64,

    /// Cumulative upload bytes (from the `traffic` sub-store)
    #[serde(default)]
    pub traffic_ul: i64,
}

fn default_server() -> String {
    "127.0.0.1".into()
}

fn default_port() -> i32 {
    1080
}

impl ProxyEntity {
    /// Create a new proxy entity with the given type (protocol).
    pub fn new(r#type: impl Into<String>) -> Self {
        Self {
            base: JsonStoreBase::new(),
            r#type: r#type.into(),
            display_type: None,
            display_name: None,
            name: String::new(),
            server_address: default_server(),
            server_port: default_port(),
            id: -1,
            gid: 0,
            latency_int: 0,
            latency_order: 0,
            is_working: false,
            dl_speed: None,
            ul_speed: None,
            test_country: None,
            bean_cfg: None,
            last_auto_test_time: None,
            full_test_report: None,
            traffic_dl: 0,
            traffic_ul: 0,
        }
    }

    /// Get the server address:port as a display string.
    pub fn display_address(&self) -> String {
        format!("{}:{}", self.server_address, self.server_port)
    }

    /// Get the display name (falls back to name).
    pub fn display_name_str(&self) -> String {
        self.display_name
            .clone()
            .or_else(|| {
                if self.name.is_empty() {
                    None
                } else {
                    Some(self.name.clone())
                }
            })
            .unwrap_or_default()
    }

    /// Get the protocol type as a display string.
    pub fn display_core_type(&self) -> String {
        // Map protocol type to display name
        match self.r#type.as_str() {
            "vmess" => "VMess".into(),
            "vless" => "VLESS".into(),
            "shadowsocks" => "Shadowsocks".into(),
            "trojan" => "Trojan".into(),
            "socks" => "SOCKS".into(),
            "http" => "HTTP".into(),
            "hysteria" => "Hysteria".into(),
            "hysteria2" => "Hysteria2".into(),
            "tuic" => "TUIC".into(),
            "wireguard" => "Wireguard".into(),
            "anytls" => "AnyTLS".into(),
            "shadowtls" => "ShadowTLS".into(),
            "naive" => "Naive".into(),
            "mieru" => "Mieru".into(),
            "juicity" => "Juicity".into(),
            "trusttunnel" => "TrustTunnel".into(),
            "ssh" => "SSH".into(),
            "tor" => "Tor".into(),
            "tailscale" => "Tailscale".into(),
            "custom" => "Custom".into(),
            "extracore" => "ExtraCore".into(),
            "chain" => "Chain".into(),
            _ => self.r#type.clone(),
        }
    }

    /// Get the combined type+name display string.
    pub fn display_type_and_name(&self) -> String {
        format!("{} {}", self.display_core_type(), self.display_name_str())
    }

    /// The "Test Result" column. Port of `ProxyEntity::DisplayTestResult`
    /// (a full test report, when there is one, takes its place): latency
    /// with the exit country, then whatever speeds were measured — a failed
    /// speed test stores "N/A", which is not shown.
    pub fn display_test_result(&self) -> String {
        if let Some(report) = self.full_test_report.as_deref().filter(|r| !r.is_empty()) {
            return report.to_string();
        }
        let mut parts: Vec<String> = Vec::new();
        if self.latency_int < 0 {
            parts.push("Unavailable".into());
        } else if self.latency_int > 0 {
            if let Some(country) = self.test_country.as_deref().filter(|c| !c.is_empty()) {
                parts.push(country.to_string());
            }
            parts.push(format!("{} ms", self.latency_int));
        }
        for (arrow, speed) in [("↓", &self.dl_speed), ("↑", &self.ul_speed)] {
            if let Some(s) = speed.as_deref().filter(|s| !s.is_empty() && *s != "N/A") {
                parts.push(format!("{arrow}{s}"));
            }
        }
        parts.join(" ")
    }

    /// Set the bean config from a protocol-specific value.
    pub fn set_bean<T: Serialize>(&mut self, bean: &T) {
        self.bean_cfg = Some(serde_json::to_value(bean).unwrap_or_default());
    }

    /// Get the bean config as a mutable reference.
    pub fn bean_cfg_mut(&mut self) -> &mut serde_json::Value {
        self.bean_cfg.get_or_insert_with(|| serde_json::Value::Object(serde_json::Map::new()))
    }

    /// Serialize the bean to JSON string.
    pub fn serialize_bean(&self) -> String {
        self.bean_cfg
            .as_ref()
            .map(|v| v.to_string())
            .unwrap_or_default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_display_test_result() {
        let mut p = ProxyEntity::new("vmess");
        assert_eq!(p.display_test_result(), "");
        p.latency_int = 120;
        p.test_country = Some("JP".into());
        p.dl_speed = Some("12.3 MB/s".into());
        p.ul_speed = Some("N/A".into());
        assert_eq!(p.display_test_result(), "JP 120 ms ↓12.3 MB/s");
        p.latency_int = -1;
        p.dl_speed = Some("N/A".into());
        assert_eq!(p.display_test_result(), "Unavailable");
    }
}
