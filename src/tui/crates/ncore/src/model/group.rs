//! `Group.hpp` — subscription groups and group extras.
//!
//! Mirrors `Group` and `GroupExtra` from `src/nekobox/dataStore/Group.hpp`.

use super::config_item::JsonStoreBase;
use serde::{Deserialize, Serialize};

/// A group of proxy profiles.
///
/// Mirrors `Group` from `src/nekobox/dataStore/Group.hpp`.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Group {
    #[serde(flatten)]
    pub base: JsonStoreBase,

    /// Group name
    #[serde(default)]
    pub name: String,

    /// Whether this group is archived (hidden from main view)
    #[serde(default)]
    pub archive: bool,

    /// Whether this group is a subscription (has a URL source)
    #[serde(default)]
    pub is_subscription: bool,

    /// ID of the front/primary proxy (used for auto-selection)
    #[serde(default)]
    pub front_proxy_id: i32,

    /// ID of the landing proxy
    #[serde(default)]
    pub landing_proxy_id: i32,

    /// Ordered list of profile IDs in this group
    #[serde(default)]
    pub profiles: Vec<i32>,

    /// Extra subscription settings (URL, headers, etc.)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub extra: Option<GroupExtra>,

    /// User notes / description
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub notes: Option<String>,
}

impl Group {
    /// Create a new empty group.
    pub fn new() -> Self {
        Self {
            base: JsonStoreBase::new(),
            name: String::new(),
            archive: false,
            is_subscription: false,
            front_proxy_id: -1,
            landing_proxy_id: -1,
            profiles: Vec::new(),
            extra: None,
            notes: None,
        }
    }

    /// Add a profile ID to this group's profile list.
    pub fn add_profile(&mut self, id: i32) {
        if !self.profiles.contains(&id) {
            self.profiles.push(id);
        }
    }

    /// Remove a profile ID from this group.
    pub fn remove_profile(&mut self, id: i32) -> bool {
        if let Some(pos) = self.profiles.iter().position(|&x| x == id) {
            self.profiles.remove(pos);
            true
        } else {
            false
        }
    }

    /// Check if a profile ID exists in this group.
    pub fn has_profile(&self, id: i32) -> bool {
        self.profiles.contains(&id)
    }

    /// Get user notes (falls back to name if no notes).
    pub fn get_notes(&self) -> String {
        self.notes
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

    /// Swap two profiles by index.
    pub fn swap_profiles(&mut self, idx1: usize, idx2: usize) -> bool {
        if idx1 < self.profiles.len() && idx2 < self.profiles.len() {
            self.profiles.swap(idx1, idx2);
            true
        } else {
            false
        }
    }

    /// Emplace a profile at a specific index.
    pub fn emplace_profile(&mut self, idx: usize, id: i32) {
        if idx <= self.profiles.len() {
            self.profiles.insert(idx, id);
        }
    }
}

impl Default for Group {
    fn default() -> Self {
        Self::new()
    }
}

/// Extra settings for subscription groups.
///
/// Mirrors `GroupExtra` from `src/nekobox/dataStore/Group.hpp`.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct GroupExtra {
    #[serde(default)]
    pub id: i32,

    /// Enable custom HTTP headers for subscription fetch
    #[serde(default)]
    pub enable_custom_headers: bool,

    /// Enable custom payload (body) for subscription fetch
    #[serde(default)]
    pub enable_custom_payload: bool,

    /// Enable hardware ID (HWID) for subscription fetch
    #[serde(default)]
    pub enable_hwid: bool,

    /// Custom HWID string
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub custom_hwid: Option<String>,

    /// Custom HTTP headers (key-value map)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub custom_headers: Option<std::collections::HashMap<String, String>>,

    /// Custom HTTP body/payload
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub text_payload: Option<String>,

    /// Custom JavaScript payload for response transformation
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub javascript_payload: Option<String>,

    /// Subscription URL
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub url: Option<String>,

    /// Subscription info string
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub info: Option<String>,

    /// Last subscription update timestamp (Unix epoch seconds)
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub sub_last_update: Option<i64>,

    /// Skip automatic updates for this group
    #[serde(default)]
    pub skip_auto_update: bool,
}
