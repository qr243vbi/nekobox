//! `window.ini` — the GUI's per-window settings (`Configs::windowSettings`,
//! declared in `src/nekobox/sys/Settings.h`).
//!
//! A handful of these govern behaviour the TUI shares with the GUI — the log
//! pane's auto-scroll and errors-only filter, whether deletions are confirmed,
//! whether the last profile is restarted on launch — so they are read from and
//! written back to the same file rather than duplicated in TUI-local state.
//!
//! The file is a flat Qt `QSettings` INI with a single `[General]` section.
//! Keys this struct does not model are preserved on save.

use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

pub const WINDOW_FILE: &str = "window.ini";

/// The subset of `window.ini` the TUI uses.
#[derive(Debug, Clone)]
pub struct WindowSettings {
    /// Scroll the log view to the newest line automatically.
    pub auto_scroll_log: bool,
    /// Show only lines classified as errors (added upstream in 5.11.28.3).
    pub errors_only: bool,
    /// Keep the core log at all.
    pub logs_enabled: bool,
    /// Ring-buffer size for the log view.
    pub max_log_line: usize,
    /// Ask before deleting profiles.
    pub ask_delete: bool,
    /// Restart the previously running profile on launch.
    pub remember_last_profile: bool,
    /// Show profile IDs instead of row numbers in the table's first column.
    pub show_profile_id: bool,

    /// Every key read from the file, so unmodelled settings survive a save.
    other: BTreeMap<String, String>,
    path: Option<PathBuf>,
}

impl Default for WindowSettings {
    fn default() -> Self {
        Self {
            auto_scroll_log: true,
            errors_only: false,
            logs_enabled: true,
            max_log_line: 200,
            ask_delete: true,
            remember_last_profile: true,
            show_profile_id: false,
            other: BTreeMap::new(),
            path: None,
        }
    }
}

impl WindowSettings {
    /// Load `<base>/window.ini`. A missing or malformed file yields defaults.
    pub fn load(base: &Path) -> Self {
        let path = base.join(WINDOW_FILE);
        let mut s = Self {
            path: Some(path.clone()),
            ..Default::default()
        };
        let Ok(text) = std::fs::read_to_string(&path) else {
            return s;
        };
        for line in text.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('[') || line.starts_with(';') {
                continue;
            }
            let Some((key, value)) = line.split_once('=') else {
                continue;
            };
            let (key, value) = (key.trim(), value.trim());
            let as_bool = value == "true";
            match key {
                "auto_scroll_log" => s.auto_scroll_log = as_bool,
                "errors_only" => s.errors_only = as_bool,
                "logs_enabled" => s.logs_enabled = as_bool,
                "ask_delete" => s.ask_delete = as_bool,
                "remember_last_profile" => s.remember_last_profile = as_bool,
                "show_profile_id" => s.show_profile_id = as_bool,
                "max_log_line" => {
                    if let Ok(n) = value.parse() {
                        s.max_log_line = n;
                    }
                }
                _ => {
                    s.other.insert(key.to_string(), value.to_string());
                }
            }
        }
        s
    }

    /// Write the file back, preserving keys this struct does not model.
    pub fn save(&self) -> anyhow::Result<()> {
        let Some(path) = &self.path else {
            return Ok(());
        };
        let mut all = self.other.clone();
        let mut put = |k: &str, v: String| {
            all.insert(k.to_string(), v);
        };
        put("auto_scroll_log", self.auto_scroll_log.to_string());
        put("errors_only", self.errors_only.to_string());
        put("logs_enabled", self.logs_enabled.to_string());
        put("ask_delete", self.ask_delete.to_string());
        put("remember_last_profile", self.remember_last_profile.to_string());
        put("show_profile_id", self.show_profile_id.to_string());
        put("max_log_line", self.max_log_line.to_string());

        let mut out = String::from("[General]\n");
        for (k, v) in &all {
            out.push_str(k);
            out.push('=');
            out.push_str(v);
            out.push('\n');
        }
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        std::fs::write(path, out)?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip_preserves_unknown_keys() {
        let dir = std::env::temp_dir().join(format!("nekotui-win-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        std::fs::write(
            dir.join(WINDOW_FILE),
            "[General]\nfont_family=Noto Sans\nerrors_only=true\nmax_log_line=500\n",
        )
        .unwrap();

        let mut s = WindowSettings::load(&dir);
        assert!(s.errors_only);
        assert_eq!(s.max_log_line, 500);

        s.errors_only = false;
        s.save().unwrap();

        let text = std::fs::read_to_string(dir.join(WINDOW_FILE)).unwrap();
        assert!(text.contains("font_family=Noto Sans"), "unknown key preserved");
        assert!(text.contains("errors_only=false"));
        assert!(text.contains("max_log_line=500"));
        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn missing_file_yields_defaults() {
        let s = WindowSettings::load(Path::new("/nonexistent-nekotui-dir"));
        assert!(s.auto_scroll_log);
        assert!(!s.errors_only);
    }
}
