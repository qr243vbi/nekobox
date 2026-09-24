//! Reader for the NekoBox binary `.cfg` format.
//!
//! The C++ GUI serializes `JsonStore` objects with `QDataStream`
//! (`JsonStore::ToBytes` in `src/gharqad/dataStore/ConfigItem.cpp`):
//!
//! - File: `"NekoBox"` magic + records (the magic is added by
//!   `JsonStore::content`, not present in nested stores).
//! - Record: `QByteArray key` + `quint8 type` + value.
//!   The key is the MD5 hash of the field name (16 bytes).
//! - All integers/lengths are big-endian (QDataStream default).
//!
//! Value encodings (Qt 6 QDataStream):
//! - `QByteArray`: u32 length + raw bytes
//! - `QString`: u32 byte length + UTF-16BE data
//! - lists: u32 count + items
//! - `jsonStore`: QByteArray containing nested records (no magic)

use std::collections::HashMap;

/// Type codes from `ConfigItemType` (src/nekobox/dataStore/ConfigItem.hpp).
mod type_code {
    pub const INT: u8 = 0;
    pub const LONG: u8 = 1;
    pub const STR: u8 = 2;
    pub const BOOL: u8 = 3;
    pub const STR_LIST: u8 = 4;
    pub const INT_LIST: u8 = 5;
    pub const JSON_STORE: u8 = 6;
    pub const JSON_STORE_LIST: u8 = 7;
    pub const STR_MAP: u8 = 8;
    pub const BOOL_PTR: u8 = 9;
    pub const JSON_SHARED: u8 = 10;
    pub const DOUBLE: u8 = 11;
    pub const ENUM: u8 = 12;
}

/// A parsed config value.
#[derive(Debug, Clone, PartialEq)]
pub enum BinValue {
    Int(i32),
    Long(i64),
    Str(String),
    Bool(bool),
    StrList(Vec<String>),
    IntList(Vec<i32>),
    /// Nested JsonStore: list of (field name, value) records.
    Store(Vec<(String, BinValue)>),
    StoreList(Vec<Vec<(String, BinValue)>>),
    StrMap(Vec<(String, BinValue)>),
    Double(f64),
    /// JsonEnum: raw int value; resolved to a name via the field's
    /// enum table when converting with context ([`BinValue::to_json_named`]).
    Enum(i32),
}

impl BinValue {
    /// Convert to a `serde_json::Value` (for bean configs).
    pub fn to_json(&self) -> serde_json::Value {
        match self {
            Self::Int(v) => (*v).into(),
            Self::Long(v) => (*v).into(),
            Self::Str(v) => v.clone().into(),
            Self::Bool(v) => (*v).into(),
            Self::StrList(v) => v.clone().into(),
            Self::IntList(v) => v.clone().into(),
            Self::Double(v) => (*v).into(),
            Self::Enum(v) => (*v).into(),
            Self::Store(recs) | Self::StrMap(recs) => {
                let map: serde_json::Map<String, serde_json::Value> = recs
                    .iter()
                    .map(|(k, v)| (k.clone(), v.to_json_named(k)))
                    .collect();
                map.into()
            }
            Self::StoreList(list) => list
                .iter()
                .map(|recs| {
                    let map: serde_json::Map<String, serde_json::Value> = recs
                        .iter()
                        .map(|(k, v)| (k.clone(), v.to_json_named(k)))
                        .collect();
                    serde_json::Value::Object(map)
                })
                .collect::<Vec<_>>()
                .into(),
        }
    }

    /// Like [`BinValue::to_json`], but resolves enums using the field name.
    pub(crate) fn to_json_named(&self, field: &str) -> serde_json::Value {
        if let Self::Enum(v) = self {
            if let Some(name) = resolve_enum(field, *v) {
                return name.into();
            }
        }
        self.to_json()
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            Self::Str(s) => Some(s),
            _ => None,
        }
    }

    pub fn as_i64(&self) -> Option<i64> {
        match self {
            Self::Int(v) => Some(*v as i64),
            Self::Long(v) => Some(*v),
            Self::Enum(v) => Some(*v as i64),
            _ => None,
        }
    }

    pub fn as_bool(&self) -> Option<bool> {
        match self {
            Self::Bool(v) => Some(*v),
            _ => None,
        }
    }
}

/// Field names we know how to resolve (MD5-hashed on disk).
/// Unknown hashes are reported as `unknown_<hex>`.
const KNOWN_NAMES: &[&str] = &[
    // ProxyEntity (src/gharqad/dataStore/ProxyEntity.cpp)
    "type", "id", "gid", "yc", "dl", "ul", "report", "country", "is_working",
    "last_auto_test_time", "name", "dtype", "addr", "port", "traffic",
    // Group (src/gharqad/dataStore/Group.cpp)
    "front_proxy_id", "landing_proxy_id", "archive", "profiles", "is_subscription",
    // GroupExtra / subscription (src/nekobox/dataStore/Group.hpp)
    "enable_custom_headers", "enable_custom_payload", "enable_hwid", "custom_hwid",
    "url", "info", "sub_last_update", "skip_auto_update", "text_payload",
    "javascript_payload", "custom_headers",
    // AbstractBean base (src/gharqad/configs/proxy/AbstractBean.cpp)
    "_v", "c_cfg", "c_out", "mux", "enable_brutal", "brutal_speed",
    // Common bean fields (ShadowSocks/VMess/TrojanVLESS/Socks/HTTP/QUIC beans)
    "method", "pass", "password", "username", "plugin", "plugin_opts", "uot",
    "network", "aid", "sec", "stream", "flow", "enc", "uuid", "password2",
    "authenticated_length", "global_padding", "up_mbps", "down_mbps",
    "obfs_password", "congestion_control", "protocol", "alpn", "certificate",
    "private_key", "public_key", "pre_shared_key", "reserved", "mtu",
    "local_address", "worker", "tls", "server", "server_port",
    "zero_rtt_handshake", "heartbeat", "disable_sni", "reduce_rtt", "headers_map",
    "download_speed", "upload_speed", "hop_interval", "sniff",
    // QUICBean (hysteria/hysteria2/tuic) + AnyTLS disk keys
    "authPayloadType", "congestionControl", "udpRelayMode", "zeroRttHandshake",
    "uos", "sni",
    // V2rayStreamSettings (src/nekobox/configs/proxy/V2RayStreamSettings.hpp)
    "net", "pac_enc", "path", "host", "sni", "cert", "insecure", "headers",
    "h_type", "ed_name", "ed_len", "xhttp_mode", "xhttp_extra", "utls",
    "tls_frag", "tls_frag_fall_delay", "tls_record_frag", "pbk", "sid", "spx",
    "fp", "ech", "ech_config", "query_server_name", "enable_ech", "kcp_extra",
    "tti", "uplinkcapacity", "downlinkcapacity", "congestion", "readbuffersize",
    "writebuffersize", "headertype", "seed",
    // Hysteria/TUIC/legacy bean fields
    "allowInsecure", "authPayload", "caText", "connectionReceiveWindow",
    "disableMtuDiscovery", "disableSni", "downloadMbps", "forceExternal",
    "lastup", "min_idle_session", "obfsPassword", "server_ports",
    "session_idle_check_interval", "session_idle_timeout", "streamReceiveWindow",
    "uploadMbps",
    // RouteRule / RoutingChain (src/gharqad/dataStore/RouteEntity.cpp)
    "update_url", "skip_update", "default_outbound", "rules", "ip_version",
    "inbound", "outbound", "domain", "domain_suffix", "domain_keyword",
    "domain_regex", "source_ip_cidr", "source_ip_is_private", "ip_cidr",
    "ip_is_private", "source_port", "source_port_range", "process_path",
    "process_path_regex", "process_name", "process_name_regex", "rule_set",
    "invert", "outboundID", "actionType", "rejectMethod", "noDrop",
    "override_address", "override_port", "sniffers", "sniffOverrideDest",
    "strategy", "simple_action", "balancers", "dns", "rawJson", "port_range",
    // TrafficData (src/nekobox/dataStore/TrafficData.hpp)
    // ("dl"/"ul" already listed above)
    // DataStore (src/gharqad/dataStore/Configs.cpp)
    "proxy", "direct", "block", "sub_custom_hwid_params", "user_agent2",
    "test_url", "disable_tray", "current_group", "inbound_address",
    "inbound_socks_port", "random_inbound_port", "log_level", "mux_protocol",
    "mux_concurrency", "mux_padding", "download_retries", "download_timeout",
    "mux_default_on", "test_concurrent", "ruleset_json_url", "inbound_username",
    "inbound_password", "core_use_uds", "custom_inbound", "custom_route",
    "network_use_proxy", "remember_id", "spmode2", "skip_cert", "hk_mw",
    "hk_group", "hk_route", "hk_spmenu", "hk_toggle", "fakedns",
    "active_routing", "data_store_type", "disable_traffic_stats", "vpn_stack",
    "vpn_mtu", "vpn_ipv6", "vpn_strict_route", "sub_rm_invalid", "sub_url_test",
    "sub_rm_duplicates", "sub_rm_unavailable", "net_insecure", "sub_auto_update",
    "sub_clear", "sub_send_hwid", "start_minimal", "max_log_line",
    "splitter_state", "utlsFingerprint", "core_box_clash_api",
    "core_box_clash_listen_addr", "core_box_clash_api_secret",
    "core_box_underlying_dns", "enable_ntp", "ntp_server_address",
    "ntp_server_port", "ntp_interval", "enable_dns_server",
    "dns_server_listen_lan", "dns_server_listen_port", "dns_v4_resp",
    "dns_v6_resp", "dns_server_rules", "enable_redirect",
    "redirect_listen_address", "redirect_listen_port", "system_dns_set",
    "windows_set_admin", "disable_win_admin", "enable_stats", "stats_tab",
    "proxy_scheme", "inbound_proxy_scheme", "disable_privilege_req",
    "enable_tun_routing", "speed_test_mode", "use_mozilla_certs",
    "adblock_enable", "speedtest_timeout_ms", "urltest_timeout_ms",
    "show_system_dns", "cache_database_name", "simple_dl_url", "auto_test_enable",
    "auto_test_interval_seconds", "auto_test_proxy_count",
    "auto_test_working_pool_size", "auto_test_latency_threshold_ms",
    "auto_test_failure_retry_count", "auto_test_target_url",
    "auto_test_tun_failover", "auto_redirect", "route_exclude_addrs",
    "tun_address", "tun_address_6", "current_route_id", "imported_group",
    "remote_dns", "remote_dns_strategy", "direct_dns", "direct_dns_strategy",
    "domain_strategy", "outbound_domain_strategy", "sniffing_mode",
    "ruleset_mirror", "use_dns_object", "dns_object", "dns_final_out_direct",
    "tun_split",
];

pub const MAGIC: &[u8] = b"NekoBox";

/// Convert a record list back into a JSON object, resolving enum fields to
/// their string names (which the C++ `JsonEnum` accepts on load).
pub fn records_to_json(records: &[(String, BinValue)]) -> serde_json::Map<String, serde_json::Value> {
    records
        .iter()
        .map(|(k, v)| (k.clone(), v.to_json_named(k)))
        .collect()
}

/// Check whether the file content is the binary format (vs JSON).
pub fn is_binary(data: &[u8]) -> bool {
    data.len() > MAGIC.len() && &data[..MAGIC.len()] == MAGIC
}

/// Parse a binary `.cfg` payload (with or without the "NekoBox" magic)
/// into a list of (field name, value) records.
pub fn parse(data: &[u8]) -> anyhow::Result<Vec<(String, BinValue)>> {
    let data = if is_binary(data) {
        &data[MAGIC.len()..]
    } else {
        data
    };
    let mut r = Reader { buf: data, pos: 0 };
    let names = NameMap::new();
    let mut out = Vec::new();
    while r.remaining() > 0 {
        let key = r.byte_array()?;
        let type_ = r.u8()?;
        let value = read_value(&mut r, type_)?;
        out.push((names.resolve(&key), value));
    }
    Ok(out)
}

fn read_value(r: &mut Reader<'_>, type_: u8) -> anyhow::Result<BinValue> {
    Ok(match type_ {
        type_code::INT => BinValue::Int(r.i32()?),
        type_code::LONG => BinValue::Long(r.i64()?),
        type_code::STR => BinValue::Str(r.qstring()?),
        type_code::BOOL | type_code::BOOL_PTR => BinValue::Bool(r.u8()? != 0),
        type_code::STR_LIST => {
            let count = r.u32()?;
            let mut list = Vec::with_capacity(count.min(1 << 16) as usize);
            for _ in 0..count {
                list.push(r.qstring()?);
            }
            BinValue::StrList(list)
        }
        type_code::INT_LIST => {
            let count = r.u32()?;
            let mut list = Vec::with_capacity(count.min(1 << 16) as usize);
            for _ in 0..count {
                list.push(r.i32()?);
            }
            BinValue::IntList(list)
        }
        type_code::JSON_STORE | type_code::JSON_SHARED => {
            let nested = r.byte_array()?;
            BinValue::Store(parse(&nested)?)
        }
        type_code::JSON_STORE_LIST => {
            let count = r.u32()?;
            let mut list = Vec::with_capacity(count.min(1 << 16) as usize);
            for _ in 0..count {
                let nested = r.byte_array()?;
                list.push(parse(&nested)?);
            }
            BinValue::StoreList(list)
        }
        type_code::STR_MAP => {
            let count = r.u32()?;
            let mut map = Vec::with_capacity(count.min(1 << 16) as usize);
            for _ in 0..count {
                let key = r.qstring()?;
                let value = r.variant()?;
                map.push((key, value));
            }
            BinValue::StrMap(map)
        }
        type_code::DOUBLE => BinValue::Double(r.f64()?),
        type_code::ENUM => {
            // JsonEnum::operator QByteArray: '\0' prefix + raw little-endian int.
            let bytes = r.byte_array()?;
            let value = if bytes.len() == 5 && bytes[0] == 0 {
                i32::from_le_bytes(bytes[1..5].try_into().unwrap())
            } else {
                0
            };
            BinValue::Enum(value)
        }
        other => anyhow::bail!("unknown config item type: {other}"),
    })
}

/// Enum field name → (ordered names, index offset), mirroring the
/// `ADD_ENUM_LIST` tables in the C++ code (Preset::SingBox).
fn enum_table(field: &str) -> Option<(&'static [&'static str], i32)> {
    Some(match field {
        // NetworkEnum: Preset::SingBox::Network, offset 1
        "network" => (&["tcp", "udp"], 1),
        // V2RAYTransportsEnum, offset 0
        "net" => (
            &["tcp", "http", "grpc", "quic", "httpupgrade", "ws", "xhttp", "kcp"],
            0,
        ),
        // VmessPacketEncodingsEnum, offset 0
        "pac_enc" => (&["", "packetaddr", "xudp"], 0),
        // QUICEnum (congestion control), offset 1
        "quic_congestion_control" => (
            &["bbr", "bbr2", "cubic", "reno", "bbr_standard", "bbr_variant"],
            1,
        ),
        // ObfsModeEnum, offset 0
        "obfs" => (&["", "http", "tls"], 0),
        // VpnImplementation (DataStore vpn_stack), offset 1
        "vpn_stack" => (&["system", "gvisor", "mixed"], 1),
        _ => return None,
    })
}

/// Resolve an enum field to its string name ("" for out-of-range).
pub fn resolve_enum(field: &str, value: i32) -> Option<&'static str> {
    let (names, offset) = enum_table(field)?;
    let idx = value - offset;
    if idx < 0 {
        return Some("");
    }
    names.get(idx as usize).copied()
}

// ============================================================================
// QDataStream primitives (big-endian)
// ============================================================================

struct Reader<'a> {
    buf: &'a [u8],
    pos: usize,
}

impl<'a> Reader<'a> {
    fn remaining(&self) -> usize {
        self.buf.len() - self.pos
    }

    fn take(&mut self, n: usize) -> anyhow::Result<&'a [u8]> {
        if self.remaining() < n {
            anyhow::bail!("unexpected end of data at {} (need {n})", self.pos);
        }
        let s = &self.buf[self.pos..self.pos + n];
        self.pos += n;
        Ok(s)
    }

    fn u8(&mut self) -> anyhow::Result<u8> {
        Ok(self.take(1)?[0])
    }

    fn u32(&mut self) -> anyhow::Result<u32> {
        Ok(u32::from_be_bytes(self.take(4)?.try_into().unwrap()))
    }

    fn i32(&mut self) -> anyhow::Result<i32> {
        Ok(i32::from_be_bytes(self.take(4)?.try_into().unwrap()))
    }

    fn i64(&mut self) -> anyhow::Result<i64> {
        Ok(i64::from_be_bytes(self.take(8)?.try_into().unwrap()))
    }

    fn f64(&mut self) -> anyhow::Result<f64> {
        Ok(f64::from_be_bytes(self.take(8)?.try_into().unwrap()))
    }

    /// QByteArray: u32 length + raw bytes (0xFFFFFFFF = null → empty).
    fn byte_array(&mut self) -> anyhow::Result<Vec<u8>> {
        let len = self.u32()?;
        if len == 0xFFFFFFFF {
            return Ok(Vec::new());
        }
        Ok(self.take(len as usize)?.to_vec())
    }

    /// QString: u32 byte length + UTF-16BE data (0xFFFFFFFF = null).
    fn qstring(&mut self) -> anyhow::Result<String> {
        let bytes = self.byte_array()?;
        if bytes.len() % 2 != 0 {
            anyhow::bail!("odd-length QString at {}", self.pos);
        }
        let units: Vec<u16> = bytes
            .as_chunks::<2>()
            .0
            .iter()
            .map(|&c| u16::from_be_bytes(c))
            .collect();
        Ok(String::from_utf16_lossy(&units))
    }

    /// QVariant (Qt 6): u32 QMetaType id + optional null flag + payload.
    /// Only the common scalar types are supported.
    fn variant(&mut self) -> anyhow::Result<BinValue> {
        let type_id = self.u32()?;
        // Qt 6.0+: isNull flag follows the type id.
        let _is_null = self.u8()?;
        // QMetaType::Type ids (Qt 6)
        Ok(match type_id {
            1 => BinValue::Bool(self.u8()? != 0),       // Bool
            2 => BinValue::Int(self.i32()?),            // Int
            4 => BinValue::Long(self.i64()?),           // LongLong
            6 => BinValue::Double(self.f64()?),         // Double
            10 => BinValue::Str(self.qstring()?),       // QString
            12 => {                                     // QByteArray
                let b = self.byte_array()?;
                BinValue::Str(String::from_utf8_lossy(&b).into_owned())
            }
            other => anyhow::bail!("unsupported QVariant type id: {other}"),
        })
    }
}

// ============================================================================
// MD5 field-name resolution
// ============================================================================

struct NameMap {
    map: HashMap<[u8; 16], &'static str>,
}

fn md5_of(s: &str) -> [u8; 16] {
    use md5::Digest;
    md5::Md5::digest(s.as_bytes()).into()
}

impl NameMap {
    fn new() -> Self {
        let map = KNOWN_NAMES
            .iter()
            .map(|name| (md5_of(name), *name))
            .collect();
        Self { map }
    }

    fn resolve(&self, hash: &[u8]) -> String {
        if hash.len() == 16 {
            let key: &[u8; 16] = hash.try_into().unwrap();
            if let Some(name) = self.map.get(key) {
                return name.to_string();
            }
        }
        format!("unknown_{}", hex(hash))
    }
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Build a record: key + type + value bytes.
    fn record(name: &str, type_: u8, value: &[u8]) -> Vec<u8> {
        let mut out = Vec::new();
        let key = md5_of(name);
        out.extend_from_slice(&(key.len() as u32).to_be_bytes());
        out.extend_from_slice(&key);
        out.push(type_);
        out.extend_from_slice(value);
        out
    }

    fn qstring(s: &str) -> Vec<u8> {
        let utf16: Vec<u8> = s
            .encode_utf16()
            .flat_map(|u| u.to_be_bytes())
            .collect();
        let mut out = (utf16.len() as u32).to_be_bytes().to_vec();
        out.extend_from_slice(&utf16);
        out
    }

    #[test]
    fn test_parse_str_and_int() {
        let mut data = MAGIC.to_vec();
        data.extend(record("name", type_code::STR, &qstring("hello")));
        data.extend(record("port", type_code::INT, &443i32.to_be_bytes()));

        let recs = parse(&data).unwrap();
        assert_eq!(recs.len(), 2);
        assert_eq!(recs[0], ("name".into(), BinValue::Str("hello".into())));
        assert_eq!(recs[1], ("port".into(), BinValue::Int(443)));
    }

    #[test]
    fn test_parse_int_list_and_nested_store() {
        let mut ids = (3u32).to_be_bytes().to_vec();
        for i in [10, 11, 12] {
            ids.extend_from_slice(&i32::to_be_bytes(i));
        }

        let mut nested = Vec::new();
        nested.extend(record("method", type_code::STR, &qstring("aes-256-gcm")));
        let mut nested_val = (nested.len() as u32).to_be_bytes().to_vec();
        nested_val.extend_from_slice(&nested);

        let mut data = MAGIC.to_vec();
        data.extend(record("profiles", type_code::INT_LIST, &ids));
        data.extend(record("stream", type_code::JSON_STORE, &nested_val));

        let recs = parse(&data).unwrap();
        assert_eq!(recs[0].1, BinValue::IntList(vec![10, 11, 12]));
        match &recs[1].1 {
            BinValue::Store(inner) => {
                assert_eq!(inner[0].0, "method");
                assert_eq!(inner[0].1, BinValue::Str("aes-256-gcm".into()));
            }
            other => panic!("expected nested store, got {other:?}"),
        }
    }

    #[test]
    fn test_utf16_nonascii() {
        let mut data = MAGIC.to_vec();
        data.extend(record("name", type_code::STR, &qstring("Gemini-GPT план")));
        let recs = parse(&data).unwrap();
        assert_eq!(recs[0].1, BinValue::Str("Gemini-GPT план".into()));
    }
}
