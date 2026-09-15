//! `TrafficData.hpp` — traffic statistics tracking.
//!
//! Mirrors `TrafficData` from `src/nekobox/dataStore/TrafficData.hpp` and the
//! accumulation loop in `src/gharqad/stats/traffic/TrafficLooper.cpp`.
//!
//! **The core reports deltas, not totals.** `QueryStats` walks the clash
//! traffic manager's per-outbound counters via `TotalOutbound(tag)`, which is
//! `uploadMap[tag].Swap(0)` — reading drains the counter. So every poll
//! returns the bytes moved *since the previous poll*, and a second poll with
//! no traffic in between returns zero. `TrafficLooper::UpdateAll` accumulates
//! (`item->uplink += up`) and derives the rate from the poll interval; this
//! type does the same.

use serde::{Deserialize, Serialize};

/// Traffic data for a single outbound tag.
///
/// Accumulates the per-poll deltas the core hands out and keeps the last
/// measured rate for display.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TrafficData {
    /// Traffic data name (e.g., "proxy", "direct", or proxy name)
    #[serde(default)]
    pub name: String,

    /// Upload bytes (cumulative over the session)
    #[serde(default)]
    pub up: i64,

    /// Download bytes (cumulative over the session)
    #[serde(default)]
    pub down: i64,

    /// Upload rate in bytes per second, from the last poll interval
    #[serde(default)]
    pub up_rate: f64,

    /// Download rate in bytes per second, from the last poll interval
    #[serde(default)]
    pub down_rate: f64,
}

impl TrafficData {
    /// Create a new traffic data tracker.
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            up: 0,
            down: 0,
            up_rate: 0.0,
            down_rate: 0.0,
        }
    }

    /// Fold in one `QueryStats` sample.
    ///
    /// `up`/`down` are the deltas the core reported for this tag and
    /// `interval_ms` is the time since the previous sample. A non-positive
    /// interval only updates the totals — matching `TrafficLooper::UpdateAll`,
    /// which skips the rate when the elapsed timer has not moved.
    pub fn add_delta(&mut self, up: i64, down: i64, interval_ms: i64) {
        self.up += up;
        self.down += down;
        if interval_ms > 0 {
            self.up_rate = up as f64 * 1000.0 / interval_ms as f64;
            self.down_rate = down as f64 * 1000.0 / interval_ms as f64;
        }
    }

    /// Zero the rates without touching the totals (used when the core stops
    /// reporting, so the status line does not freeze at the last speed).
    pub fn clear_rates(&mut self) {
        self.up_rate = 0.0;
        self.down_rate = 0.0;
    }

    /// Formatted upload speed (e.g. `"1.2 MB/s"`).
    pub fn up_speed(&self) -> String {
        format_bytes(self.up_rate.max(0.0) as u64, true)
    }

    /// Formatted download speed.
    pub fn down_speed(&self) -> String {
        format_bytes(self.down_rate.max(0.0) as u64, true)
    }

    /// Get both formatted speed strings.
    pub fn speeds(&self) -> (String, String) {
        (self.up_speed(), self.down_speed())
    }
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

    /// `QueryStats` hands out deltas that are drained on read, so repeated
    /// samples must add up rather than overwrite.
    #[test]
    fn test_deltas_accumulate() {
        let mut t = TrafficData::new("proxy");
        t.add_delta(75, 870, 1000);
        t.add_delta(75, 870, 1000);
        assert_eq!(t.up, 150);
        assert_eq!(t.down, 1740);
    }

    #[test]
    fn test_rate_from_interval() {
        let mut t = TrafficData::new("proxy");
        // 2048 bytes over half a second is 4096 B/s.
        t.add_delta(0, 2048, 500);
        assert_eq!(t.down_rate, 4096.0);
        assert_eq!(t.down_speed(), "4.0 KB/s");

        // An idle interval drops the rate back to zero but keeps the total.
        t.add_delta(0, 0, 1000);
        assert_eq!(t.down_rate, 0.0);
        assert_eq!(t.down, 2048);
    }

    /// A zero interval must not divide by zero or wipe the last known rate.
    #[test]
    fn test_zero_interval_keeps_rate() {
        let mut t = TrafficData::new("proxy");
        t.add_delta(0, 1000, 1000);
        t.add_delta(0, 500, 0);
        assert_eq!(t.down_rate, 1000.0);
        assert_eq!(t.down, 1500);
    }
}
