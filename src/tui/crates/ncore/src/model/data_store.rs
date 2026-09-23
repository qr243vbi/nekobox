//! `DataStore.hpp` — global application settings.
//!
//! Mirrors `DataStore` and `Routing` from `src/nekobox/dataStore/DataStore.hpp`.

use serde::{Deserialize, Serialize};

/// Global application settings.
///
/// Mirrors `DataStore` from `src/nekobox/dataStore/DataStore.hpp`.
/// This is the top-level config file (`nekobox.cfg` in the data directory).
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct DataStore {
    /// Core RPC settings
    #[serde(default)]
    pub core_use_uds: bool,

    #[serde(default = "default_core_port")]
    pub core_port: i32,

    #[serde(default = "default_core_domain")]
    pub core_domain: String,

    /// Currently started profile ID (or -1919 = none)
    #[serde(default)]
    pub started_id: i32,

    /// Whether the core is currently running
    #[serde(default)]
    pub core_running: bool,

    /// Proxy entity data (keyed by ID)
    #[serde(default)]
    pub profiles: std::collections::HashMap<i32, serde_json::Value>,

    /// Group data (keyed by ID)
    #[serde(default)]
    pub groups: std::collections::HashMap<i32, serde_json::Value>,

    /// Routing chain data (keyed by ID)
    #[serde(default)]
    pub routes: std::collections::HashMap<i32, serde_json::Value>,

    /// Current routing chain name
    #[serde(default = "default_active_routing")]
    pub active_routing: String,

    /// Active routing chain id (`current_route_id` in `default_route_profile.cfg`).
    ///
    /// This — not [`Self::active_routing`] — is what the GUI reads when it
    /// builds a config, so it is the field the TUI must keep in sync.
    #[serde(default = "default_current_route_id")]
    pub current_route_id: i32,

    /// `imported_group` in `default_route_profile.cfg`: the group that
    /// outbounds referenced by routing rules get imported into.
    #[serde(default = "default_imported_group")]
    pub imported_group: i32,

    /// Sniffing mode (`sniffing_mode`): 0=disabled, 1=for routing, 2=full.
    #[serde(default = "default_sniffing_mode")]
    pub sniffing_mode: i32,

    /// Remembered special-proxy modes (`spmode2`): any of "vpn", "system_proxy".
    #[serde(default)]
    pub remember_spmode: Vec<String>,

    /// Current group ID
    #[serde(default)]
    pub current_group: i32,

    /// Inbound listen address
    #[serde(default = "default_inbound_address")]
    pub inbound_address: String,

    /// Inbound port (SOCKS/mixed)
    #[serde(default = "default_inbound_port")]
    pub inbound_socks_port: i32,

    /// Inbound username (if auth enabled)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub inbound_username: Option<String>,

    /// Inbound password (if auth enabled)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub inbound_password: Option<String>,

    /// Whether to randomize inbound port
    #[serde(default)]
    pub random_inbound_port: bool,

    /// Custom inbound JSON
    #[serde(default = "default_custom_inbound")]
    pub custom_inbound: String,

    /// Remote DNS address
    #[serde(default = "default_remote_dns")]
    pub remote_dns: String,

    /// Remote DNS resolution strategy
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub remote_dns_strategy: Option<String>,

    /// Direct DNS address
    #[serde(default = "default_direct_dns")]
    pub direct_dns: String,

    /// Direct DNS resolution strategy
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub direct_dns_strategy: Option<String>,

    /// Use DNS object in sing-box config
    #[serde(default)]
    pub use_dns_object: bool,

    /// DNS object address
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub dns_object: Option<String>,

    /// DNS final outbound: direct
    #[serde(default)]
    pub dns_final_out_direct: bool,

    /// Domain strategy: "AsIs", "ipv4_only", "ipv6_only", "prefer_ipv4", "prefer_ipv6"
    #[serde(default = "default_domain_strategy")]
    pub domain_strategy: String,

    /// Outbound domain strategy
    #[serde(default = "default_outbound_domain_strategy")]
    pub outbound_domain_strategy: String,

    /// TUN interface name
    #[serde(default)]
    pub tun_name: String,

    /// TUN interface address (IPv4)
    #[serde(default = "default_tun_address")]
    pub tun_address: String,

    /// TUN interface address (IPv6)
    #[serde(default = "default_tun_address_6")]
    pub tun_address_6: String,

    /// Enable TUN routing
    #[serde(default)]
    pub enable_tun_routing: bool,

    /// VPN implementation: "system", "gvisor", "mixed"
    #[serde(default = "default_vpn_implementation")]
    pub vpn_implementation: String,

    /// VPN MTU
    #[serde(default = "default_vpn_mtu")]
    pub vpn_mtu: i32,

    /// VPN IPv6
    #[serde(default)]
    pub vpn_ipv6: bool,

    /// VPN strict route
    #[serde(default)]
    pub vpn_strict_route: bool,

    /// Fake DNS
    #[serde(default)]
    pub fake_dns: bool,

    /// Disable privilege requirement check
    #[serde(default)]
    pub disable_privilege_req: bool,

    /// Proxy for HTTP requests (subscription fetch)
    #[serde(default = "default_network_use_proxy")]
    pub network_use_proxy: bool,

    /// Insecure skip TLS verify for HTTP requests
    #[serde(default)]
    pub net_insecure: bool,

    /// User agent for HTTP requests
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub user_agent: Option<String>,

    /// Ruleset JSON URL
    #[serde(default = "default_ruleset_json_url")]
    pub ruleset_json_url: String,

    /// Ruleset mirror: 0=Cloudflare, 1=GitHub
    #[serde(default)]
    pub ruleset_mirror: i32,

    /// Log level: "debug", "info", "warning", "error"
    #[serde(default = "default_log_level")]
    pub log_level: String,

    /// Test latency URL
    #[serde(default = "default_test_latency_url")]
    pub test_latency_url: String,

    /// URL test timeout (ms)
    #[serde(default = "default_url_test_timeout_ms")]
    pub url_test_timeout_ms: i32,

    /// Speed test timeout (ms)
    #[serde(default = "default_speed_test_timeout_ms")]
    pub speed_test_timeout_ms: i32,

    /// Speed test mode: 0=full, 1=download, 2=upload, 3=latency
    #[serde(default)]
    pub speed_test_mode: i32,

    /// Simple download URL
    #[serde(default = "default_simple_dl_url")]
    pub simple_dl_url: String,

    /// URL test concurrency
    #[serde(default = "default_test_concurrent")]
    pub test_concurrent: i32,

    /// Test latency URL for proxy
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub auto_test_target_url: Option<String>,

    /// Auto-test interval (seconds)
    #[serde(default)]
    pub auto_test_interval_seconds: i32,

    /// Auto-test proxy count
    #[serde(default)]
    pub auto_test_proxy_count: i32,

    /// Auto-test working pool size
    #[serde(default)]
    pub auto_test_working_pool_size: i32,

    /// Auto-test latency threshold (ms)
    #[serde(default)]
    pub auto_test_latency_threshold_ms: i32,

    /// Auto-test failure retry count
    #[serde(default)]
    pub auto_test_failure_retry_count: i32,

    /// Enable auto-test
    #[serde(default)]
    pub auto_test_enable: bool,

    /// Auto-test TUN failover
    #[serde(default)]
    pub auto_test_tun_failover: bool,

    /// Subscriptions auto-update interval (hours, negative = disabled)
    #[serde(default = "default_sub_auto_update")]
    pub sub_auto_update: i32,

    /// Subscriptions: clear after update
    #[serde(default)]
    pub sub_clear: bool,

    /// Subscriptions: send HWID
    #[serde(default)]
    pub sub_send_hwid: bool,

    /// Subscriptions: remove unavailable after URL test
    #[serde(default)]
    pub sub_rm_unavailable: bool,

    /// Subscriptions: remove duplicates
    #[serde(default)]
    pub sub_rm_duplicates: bool,

    /// Subscriptions: run URL test during update
    #[serde(default)]
    pub sub_url_test: bool,

    /// Subscriptions: remove invalid links
    #[serde(default)]
    pub sub_rm_invalid: bool,

    /// Skip certificate verification
    #[serde(default)]
    pub skip_cert: bool,

    /// uTLS fingerprint
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub utls_fingerprint: Option<String>,

    /// Mux protocol: "smux", "yamux", "h2mux"
    #[serde(default = "default_mux_protocol")]
    pub mux_protocol: String,

    /// Mux padding
    #[serde(default)]
    pub mux_padding: bool,

    /// Mux concurrency
    #[serde(default = "default_mux_concurrency")]
    pub mux_concurrency: i32,

    /// Mux default on
    #[serde(default)]
    pub mux_default_on: bool,

    /// Download retries
    #[serde(default = "default_download_retries")]
    pub download_retries: i32,

    /// Download timeout (ms)
    #[serde(default = "default_download_timeout")]
    pub download_timeout: i32,

    /// Connection statistics (`enable_stats` on disk). Enables the Clash API
    /// in the generated config, without which the core cannot list
    /// connections. Defaults to true, as in the C++ `DataStore`.
    #[serde(default = "default_true")]
    pub connection_statistics: bool,

    /// Stats tab index
    #[serde(default)]
    pub stats_tab: i32,

    /// Disable traffic stats
    #[serde(default)]
    pub disable_traffic_stats: bool,

    /// Disable tray icon
    #[serde(default)]
    pub disable_tray: bool,

    /// Custom route global JSON
    #[serde(default = "default_custom_route_global")]
    pub custom_route_global: String,

    /// Adblock enable
    #[serde(default)]
    pub adblock_enable: bool,

    /// Use Mozilla certs
    #[serde(default)]
    pub use_mozilla_certs: bool,

    /// VPN implementation (alias)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub vpn_implementation_override: Option<String>,

    /// Route exclude addresses
    #[serde(default = "default_route_exclude_addrs")]
    pub route_exclude_addrs: Vec<String>,

    /// Allow beta updates
    #[serde(default)]
    pub allow_beta_update: bool,

    /// System DNS set flag
    #[serde(default)]
    pub system_dns_set: bool,

    /// Windows: set admin
    #[serde(default)]
    pub windows_set_admin: bool,

    /// Windows: no admin mode
    #[serde(default)]
    pub windows_no_admin: bool,

    /// NTP settings
    #[serde(default)]
    pub enable_ntp: bool,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub ntp_server_address: Option<String>,
    #[serde(default)]
    pub ntp_server_port: i32,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub ntp_interval: Option<String>,

    /// DNS server settings
    #[serde(default)]
    pub enable_dns_server: bool,
    #[serde(default)]
    pub dns_server_listen_lan: bool,
    #[serde(default = "default_dns_server_port")]
    pub dns_server_listen_port: i32,
    #[serde(default = "default_dns_v4_resp")]
    pub dns_v4_resp: String,
    #[serde(default = "default_dns_v6_resp")]
    pub dns_v6_resp: String,
    #[serde(default)]
    pub dns_server_rules: Vec<String>,

    /// HTTP redirect
    #[serde(default)]
    pub enable_redirect: bool,
    #[serde(default = "default_redirect_listen_address")]
    pub redirect_listen_address: String,
    #[serde(default = "default_redirect_listen_port")]
    pub redirect_listen_port: i32,

    /// Tun split (proxy/direct/block IP lists)
    #[serde(default)]
    pub tun_split: TunSplit,

    /// Clash API settings
    #[serde(default = "default_clash_api_port")]
    pub core_box_clash_api: i32,
    #[serde(default = "default_clash_listen_addr")]
    pub core_box_clash_listen_addr: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub core_box_clash_api_secret: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub core_box_underlying_dns: Option<String>,
}

// --- TunSplit ---

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct TunSplit {
    #[serde(default)]
    pub proxy: Vec<String>,
    #[serde(default)]
    pub direct: Vec<String>,
    #[serde(default)]
    pub block: Vec<String>,
}

// --- Default values ---

fn default_core_port() -> i32 { 19810 }
fn default_core_domain() -> String { "127.0.0.1".into() }
fn default_inbound_address() -> String { "127.0.0.1".into() }
fn default_inbound_port() -> i32 { 2080 }
fn default_remote_dns() -> String { "tls://8.8.8.8".into() }
fn default_direct_dns() -> String { "localhost".into() }
fn default_domain_strategy() -> String { "AsIs".into() }
fn default_outbound_domain_strategy() -> String { "AsIs".into() }
fn default_tun_address() -> String { "172.19.0.1/24".into() }
fn default_tun_address_6() -> String { "fdfe:dcba:9876::1/96".into() }
fn default_vpn_implementation() -> String { "gvisor".into() }
fn default_vpn_mtu() -> i32 { 1500 }
fn default_network_use_proxy() -> bool { true }
fn default_ruleset_json_url() -> String {
    "https://github.com/qr243vbi/ruleset/raw/refs/heads/rule-set/srslist.json".into()
}
fn default_log_level() -> String { "info".into() }
fn default_test_latency_url() -> String { "http://cp.cloudflare.com/".into() }
fn default_url_test_timeout_ms() -> i32 { 6000 }
fn default_speed_test_timeout_ms() -> i32 { 5000 }
fn default_simple_dl_url() -> String { "http://cachefly.cachefly.net/1mb.test".into() }
fn default_test_concurrent() -> i32 { 10 }
fn default_sub_auto_update() -> i32 { -30 }
fn default_mux_protocol() -> String { "smux".into() }
fn default_mux_concurrency() -> i32 { 8 }
fn default_download_retries() -> i32 { 25 }
fn default_download_timeout() -> i32 { 10000 }
fn default_custom_inbound() -> String { "{\"inbounds\": []}".into() }
fn default_active_routing() -> String { "Default".into() }
fn default_true() -> bool { true }
fn default_current_route_id() -> i32 { 1 }
fn default_imported_group() -> i32 { -1 }
fn default_sniffing_mode() -> i32 { 1 }
fn default_custom_route_global() -> String { "{\"rules\": []}".into() }
fn default_route_exclude_addrs() -> Vec<String> {
    vec![
        "127.0.0.0/8".into(),
        "10.0.0.0/8".into(),
        "172.16.0.0/12".into(),
        "192.168.0.0/16".into(),
        "169.254.0.0/16".into(),
        "224.0.0.0/4".into(),
        "255.255.255.255/32".into(),
    ]
}
fn default_dns_server_port() -> i32 { 53 }
fn default_dns_v4_resp() -> String { "127.0.0.1".into() }
fn default_dns_v6_resp() -> String { "::1".into() }
fn default_redirect_listen_address() -> String { "127.0.0.1".into() }
fn default_redirect_listen_port() -> i32 { 443 }
fn default_clash_api_port() -> i32 { -9090 }
fn default_clash_listen_addr() -> String { "127.0.0.1".into() }

impl Default for DataStore {
    fn default() -> Self {
        Self {
            core_use_uds: cfg!(target_os = "linux"),
            core_port: default_core_port(),
            core_domain: default_core_domain(),
            started_id: -1919,
            core_running: false,
            profiles: std::collections::HashMap::new(),
            groups: std::collections::HashMap::new(),
            routes: std::collections::HashMap::new(),
            active_routing: default_active_routing(),
            current_route_id: default_current_route_id(),
            imported_group: default_imported_group(),
            sniffing_mode: default_sniffing_mode(),
            remember_spmode: Vec::new(),
            current_group: 0,
            inbound_address: default_inbound_address(),
            inbound_socks_port: default_inbound_port(),
            inbound_username: None,
            inbound_password: None,
            random_inbound_port: false,
            custom_inbound: default_custom_inbound(),
            remote_dns: default_remote_dns(),
            remote_dns_strategy: None,
            direct_dns: default_direct_dns(),
            direct_dns_strategy: None,
            use_dns_object: false,
            dns_object: None,
            dns_final_out_direct: false,
            domain_strategy: default_domain_strategy(),
            outbound_domain_strategy: default_outbound_domain_strategy(),
            tun_address: default_tun_address(),
            tun_address_6: default_tun_address_6(),
            enable_tun_routing: false,
            vpn_implementation: default_vpn_implementation(),
            vpn_mtu: default_vpn_mtu(),
            vpn_ipv6: false,
            vpn_strict_route: true,
            fake_dns: false,
            disable_privilege_req: false,
            network_use_proxy: default_network_use_proxy(),
            net_insecure: false,
            user_agent: None,
            ruleset_json_url: default_ruleset_json_url(),
            ruleset_mirror: 0, // Cloudflare
            log_level: default_log_level(),
            test_latency_url: default_test_latency_url(),
            url_test_timeout_ms: default_url_test_timeout_ms(),
            speed_test_timeout_ms: default_speed_test_timeout_ms(),
            speed_test_mode: 0, // full
            simple_dl_url: default_simple_dl_url(),
            test_concurrent: default_test_concurrent(),
            auto_test_target_url: None,
            auto_test_interval_seconds: 0,
            auto_test_proxy_count: 0,
            auto_test_working_pool_size: 0,
            auto_test_latency_threshold_ms: 0,
            auto_test_failure_retry_count: 0,
            auto_test_enable: false,
            auto_test_tun_failover: true,
            sub_auto_update: default_sub_auto_update(),
            sub_clear: false,
            sub_send_hwid: false,
            sub_rm_unavailable: false,
            sub_rm_duplicates: false,
            sub_url_test: false,
            sub_rm_invalid: false,
            skip_cert: false,
            utls_fingerprint: None,
            mux_protocol: default_mux_protocol(),
            mux_padding: false,
            mux_concurrency: default_mux_concurrency(),
            mux_default_on: false,
            download_retries: default_download_retries(),
            download_timeout: default_download_timeout(),
            connection_statistics: true,
            stats_tab: 0,
            disable_traffic_stats: false,
            disable_tray: false,
            custom_route_global: default_custom_route_global(),
            adblock_enable: false,
            use_mozilla_certs: false,
            vpn_implementation_override: None,
            route_exclude_addrs: default_route_exclude_addrs(),
            allow_beta_update: false,
            system_dns_set: false,
            windows_set_admin: false,
            windows_no_admin: false,
            enable_ntp: false,
            ntp_server_address: None,
            ntp_server_port: 0,
            ntp_interval: None,
            enable_dns_server: false,
            dns_server_listen_lan: false,
            dns_server_listen_port: default_dns_server_port(),
            dns_v4_resp: default_dns_v4_resp(),
            dns_v6_resp: default_dns_v6_resp(),
            dns_server_rules: Vec::new(),
            enable_redirect: false,
            redirect_listen_address: default_redirect_listen_address(),
            redirect_listen_port: default_redirect_listen_port(),
            tun_split: TunSplit::default(),
            core_box_clash_api: default_clash_api_port(),
            core_box_clash_listen_addr: default_clash_listen_addr(),
            core_box_clash_api_secret: None,
            core_box_underlying_dns: None,
        }
    }
}
