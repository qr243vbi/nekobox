//! `TrafficData.hpp` — traffic statistics tracking.
//!
//! Mirrors `TrafficData` from `src/nekobox/dataStore/TrafficData.hpp`.

use serde::{Deserialize, Serialize};

/// Traffic data for a single proxy or the aggregate.
///
/// Tracks cumulative upload/download bytes and provides formatted
/// speed display strings.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TrafficData {
    /// Traffic data name (e.g., "proxy", "direct", or proxy name)
    #[serde(default)]
    pub name: String,

    /// Upload bytes (cumulative)
    #[serde(default)]
    pub up: i64,

    /// Download bytes (cumulative)
    #[serde(default)]
    pub down: i64,

    /// Previous up value for delta calculation
    #[serde(default)]
    prev_up: i64,

    /// Previous down value for delta calculation
    #[serde(default)]
    prev_down: i64,

    /// Timestamp of last update (Unix epoch seconds)
    #[serde(default)]
    last_update: i64,

    /// Formatted upload speed string (e.g., "1.2 MB/s")
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub up_speed: Option<String>,

    /// Formatted download speed string
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub down_speed: Option<String>,
}

impl TrafficData {
    /// Create a new traffic data tracker.
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            up: 0,
            down: 0,
            prev_up: 0,
            prev_down: 0,
            last_update: 0,
            up_speed: None,
            down_speed: None,
        }
    }

    /// Update with new cumulative values.
    pub fn update(&mut self, up: i64, down: i64) {
        let now = chrono::Utc::now().timestamp();
        let elapsed = if self.last_update > 0 {
            (now - self.last_update) as f64
        } else {
            1.0
        };

        self.up_speed = Some(format_bytes_per_sec(up - self.prev_up, elapsed));
        self.down_speed = Some(format_bytes_per_sec(down - self.prev_down, elapsed));

        self.prev_up = up;
        self.prev_down = down;
        self.up = up;
        self.down = down;
        self.last_update = now;
    }

    /// Get the current speed strings.
    pub fn speeds(&self) -> (Option<&str>, Option<&str>) {
        (self.up_speed.as_deref(), self.down_speed.as_deref())
    }
}

/// Format bytes per second into a human-readable string.
fn format_bytes_per_sec(bytes: i64, elapsed: f64) -> String {
    if elapsed <= 0.0 {
        return "0 B/s".into();
    }
    let bps = (bytes as f64) / elapsed;
    format_bytes(bps as u64, true)
}

/// Format bytes into a human-readable string.
pub fn format_bytes(bytes: u64, speed: bool) -> String {
    const UNITS: [&str; 7] = ["B", "KB", "MB", "GB", "TB", "PB", "EB"];
    if bytes == 0 {
        return if speed {
            "0 B/s".into()
        } else {
            "0 B".into()
        };
    }
    let bytes = bytes as f64;
    let exponent = (bytes.log2() / 10.0).floor() as usize;
    let exponent = exponent.min(UNITS.len() - 1);
    let value = bytes / 2_f64.powi((exponent * 10) as i32);
    if speed {
        format!("{:.1} {}/s", value, UNITS[exponent])
    } else {
        format!("{:.1} {}", value, UNITS[exponent])
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_format_bytes() {
        assert_eq!(format_bytes(0, false), "0 B");
        assert_eq!(format_bytes(1024, false), "1.0 KB");
        assert_eq!(format_bytes(1_048_576, false), "1.0 MB");
        assert!(format_bytes(1_048_576, true).contains("MB/s"));
    }
}
