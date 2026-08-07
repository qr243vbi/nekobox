//! `ConfigItem.hpp` / `Configs.hpp` equivalents.
//!
//! The C++ `JsonStore` is a base class with a dynamic key-value map
//! (`ConfJsMap`) that uses MD5-hashed keys. In Rust we replace this with
//! compile-time serde structs — the on-disk format is flat JSON, so we just
//! need matching field names.
//!
//! Key convention: C++ `ADD_MAP("key_name", field, type)` → `#[serde(rename = "key_name")]`

use serde::{Deserialize, Serialize};

// ============================================================================
// JsonStoreType — mirrors `JsonStoreType` enum in ConfigItem.hpp
// ============================================================================

/// Store type markers. Mirrors the C++ `JsonStoreType` enum.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[derive(Default)]
pub enum JsonStoreType {
    #[serde(rename = "Routes")]
    Routes,
    #[serde(rename = "Proxies")]
    Proxies,
    #[serde(rename = "Groups")]
    Groups,
    #[serde(rename = "Beans")]
    Beans,
    #[serde(rename = "ProxyManager")]
    ProxyManager,
    #[serde(rename = "NekoBox")]
    NekoBox,
    #[serde(rename = "DefaultRoute")]
    DefaultRoute,
    #[serde(rename = "NoSave")]
    #[default]
    NoSave,
    #[serde(rename = "Subscriptions")]
    Subscriptions,
}


// ============================================================================
// JsonStoreFlags
// ============================================================================

/// Bit flags mirroring `JsonStoreFlags` from ConfigItem.hpp.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[derive(Default)]
pub struct JsonStoreFlags {
    save_control_no_save: bool,
    storage_exists: bool,
    custom_flag: bool,
    custom_flag2: bool,
}


// ============================================================================
// Base JsonStore trait
// ============================================================================

/// Trait mirroring `JsonStore` base class.
///
/// The C++ implementation uses a dynamic `QJsonObject`-backed map.
/// In Rust we use a trait that defines the interface, and each concrete
/// type implements it with its own serde-derived struct.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct JsonStoreBase {
    #[serde(default = "default_id")]
    pub id: i32,
    #[serde(default = "default_flags")]
    pub flags: u8,
}

fn default_id() -> i32 {
    -1
}

fn default_flags() -> u8 {
    0
}

impl JsonStoreBase {
    pub fn new() -> Self {
        Self {
            id: -1,
            flags: 0,
        }
    }
}

impl Default for JsonStoreBase {
    fn default() -> Self {
        Self::new()
    }
}
