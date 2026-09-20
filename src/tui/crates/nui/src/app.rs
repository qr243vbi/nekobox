//! Application state, event loop, and key routing.
//!
//! The layout mirrors `src/nekobox/ui/mainwindow.ui` top to bottom:
//!
//! ```text
//!  Program  Profiles  Preferences  Routing  Test  Information   <- menubar
//!  [ Start ] [x] Tun  [ ] System Proxy  [ ] System DNS   search  <- control row
//!  ( Group A )( Group B )                                        <- tabWidget
//!  #  Type  Address  Name  Test Result  Traffic                  <- proxyListTable
//!  ------------------------------- splitter -------------------
//!  [Logs][Connections][Traffic Graph]                            <- stats_widget
//!  Proxy: .. Direct: ..  |  [group] profile  |  Inbound: ..      <- status labels
//! ```
//!
//! Dialogs (settings, routing, groups, statistics, about) are modal overlays,
//! matching the GUI where they are `QDialog`s rather than panes.

use crate::cli::Args;
use crate::menu::{self, Action, Menu, MenuContext, MenuState};
use crate::rpc_worker::{Command, Event, SpeedTestMode, WorkerHandle};
use crossterm::event::{self, KeyCode, KeyEventKind, KeyModifiers};
use ncore::model::{DataStore, Group, ProxyEntity, RoutingChain, TrafficData};
use ncore::window::WindowSettings;
use ratatui::prelude::*;
use ratatui::widgets::{
    Block, BorderType, Cell, Clear, Paragraph, Row, Table, TableState, Wrap,
};
use std::collections::{HashMap, HashSet, VecDeque};
use std::path::PathBuf;
use std::time::{Duration, Instant};

const STATS_INTERVAL: Duration = Duration::from_secs(1);
const URL_TEST_TIMEOUT: Duration = Duration::from_secs(45);
const SPEED_HISTORY_CAP: usize = 240;
/// How long a transient message stays in the `data_view` line.
const TRANSIENT_TTL: Duration = Duration::from_secs(8);

/// The special-proxy mode. The GUI exposes `spmode_vpn` / `spmode_system_proxy`
/// as two booleans but its menu and `set_spmode_*` helpers make them mutually
/// exclusive, so a single tri-state matches the observable behaviour.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SpMode {
    Disabled,
    SystemProxy,
    Tun,
}

impl SpMode {
    fn is_tun(self) -> bool {
        self == Self::Tun
    }
    fn is_system_proxy(self) -> bool {
        self == Self::SystemProxy
    }
}

/// The bottom panel tab (`stats_widget` in the GUI).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BottomTab {
    Logs,
    Connections,
    Graph,
}

impl BottomTab {
    pub fn label(&self) -> &'static str {
        match self {
            Self::Logs => "Logs",
            Self::Connections => "Connections",
            Self::Graph => "Traffic Graph",
        }
    }

    const ALL: [BottomTab; 3] = [Self::Logs, Self::Connections, Self::Graph];
}

/// Which half of the splitter has the keyboard.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Focus {
    Table,
    Bottom,
}

/// Core connection status.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CoreStatus {
    Connecting,
    Running,
    Stopped,
    Error(String),
}

impl std::fmt::Display for CoreStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Connecting => write!(f, "Connecting..."),
            Self::Running => write!(f, "Running"),
            Self::Stopped => write!(f, "Not Running"),
            Self::Error(msg) => write!(f, "Error: {msg}"),
        }
    }
}

/// A core log line, tagged with the upstream error classification so the
/// errors-only filter can be applied without re-matching on every frame.
#[derive(Debug, Clone)]
pub struct LogEntry {
    pub text: String,
    pub is_error: bool,
    /// How many times this line repeated in a row. A once-a-second poll that
    /// keeps failing would otherwise push everything else out of the buffer.
    pub repeats: u32,
}

impl LogEntry {
    /// The line as displayed, with the repeat counter appended.
    pub fn display(&self) -> String {
        if self.repeats > 1 {
            format!("{} (×{})", self.text, self.repeats)
        } else {
            self.text.clone()
        }
    }
}

/// Modal overlays. The GUI shows these as dialogs, so only one is open at a
/// time and it captures all input.
pub enum Dialog {
    Help,
    Settings,
    Routes,
    Groups,
    Confirm(Confirm),
    Prompt(Prompt),
    Picker(Picker),
    /// Read-only scrollable text (exported config, statistics, about).
    Text {
        title: String,
        body: String,
        scroll: u16,
    },
}

pub struct Confirm {
    pub message: String,
    pub action: ConfirmAction,
}

pub enum ConfirmAction {
    DeleteProfiles(Vec<i32>),
    DeleteGroup(i32),
    RemoveProfiles { ids: Vec<i32>, what: &'static str },
}

pub struct Prompt {
    pub title: String,
    pub buffer: String,
    pub action: PromptAction,
}

pub enum PromptAction {
    NewGroup,
    RenameGroup(i32),
    SetGroupUrl(i32),
    AddProfileFromFile,
    UpdateProfileFromFile(i32),
}

pub struct Picker {
    pub title: String,
    pub items: Vec<(String, PickAction)>,
    pub cursor: usize,
}

pub enum PickAction {
    MoveToGroup(i32),
    RouteDomain {
        domain: String,
        action: i32,
        match_type: &'static str,
    },
}

/// The application state.
pub struct App {
    pub running: bool,

    /// Base config directory (contains profiles/, groups/, nekobox.cfg)
    config_dir: PathBuf,

    /// Global settings (`nekobox.cfg` + `default_route_profile.cfg`)
    datastore: DataStore,

    /// `window.ini` settings shared with the GUI
    window: WindowSettings,

    groups: Vec<Group>,
    profiles: HashMap<i32, ProxyEntity>,
    chains: Vec<RoutingChain>,

    current_group: usize,

    /// Cursor row in the (filtered) proxy table
    selected: usize,
    table_state: TableState,

    /// Explicitly checked profile ids (the GUI's multi-row selection)
    checked: HashSet<i32>,

    latencies: HashMap<i32, i32>,

    core_status: CoreStatus,
    active_profile_id: Option<i32>,

    traffic_proxy: TrafficData,
    traffic_direct: TrafficData,
    speed_history: VecDeque<(u64, u64)>,
    connections: Vec<crate::rpc_worker::ConnectionInfo>,

    /// Tri-state special-proxy mode (`checkBox_VPN` / `checkBox_SystemProxy`)
    spmode: SpMode,
    /// `system_dns` checkbox
    system_dns: bool,

    bottom_tab: BottomTab,
    focus: Focus,

    /// Open menu, if any
    menu: Option<MenuState>,
    menu_bar: Vec<Menu>,
    /// x ranges of the menu bar titles, for mouse hit-testing
    menu_bar_xranges: Vec<(u16, u16)>,

    dialog: Option<Dialog>,

    /// Settings dialog cursor + inline edit buffer
    settings_sel: usize,
    settings_edit: Option<String>,
    /// Routes / Groups dialog cursors
    routes_sel: usize,
    groups_sel: usize,

    speed_test_running: bool,

    table_rows_area: Rect,
    group_tab_xranges: Vec<(u16, u16)>,
    bottom_tab_xranges: Vec<(u16, u16, u16)>,
    control_row_hits: Vec<(u16, u16, Action)>,
    last_click: Option<(Instant, u16, u16)>,

    logs: VecDeque<LogEntry>,
    /// Cursor into the *filtered* log view when the bottom panel has focus
    log_cursor: usize,

    /// Search box (`searchBox`) — visible + contents
    search_visible: bool,
    filter_mode: bool,
    filter: String,

    /// Transient status line (the GUI's `data_view`)
    transient: Option<(String, Instant)>,

    url_test_running: bool,
    url_test_done: HashSet<String>,
    url_test_expected: usize,
    url_test_started: Option<Instant>,

    last_stats_poll: Instant,
    /// When the stats sample currently in flight was requested — the core
    /// reports deltas, so the rate needs the real interval between samples,
    /// not the nominal `STATS_INTERVAL`.
    last_stats_at: Option<Instant>,
    worker: WorkerHandle,
}

/// Resolve the core binary: explicit arg → $PATH → repo-local build/ dir
/// (dev layout: the TUI binary lives at src/tui/target/<profile>/).
fn resolve_core_binary(arg: Option<String>) -> String {
    if let Some(b) = arg {
        return b;
    }
    if let Some(path) = std::env::var_os("PATH") {
        for dir in std::env::split_paths(&path) {
            let cand = dir.join("nekobox_core");
            if cand.is_file() {
                return cand.to_string_lossy().into_owned();
            }
        }
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let cand = dir.join("../../../../build/nekobox_core");
            if let Ok(cand) = cand.canonicalize() {
                if cand.is_file() {
                    return cand.to_string_lossy().into_owned();
                }
            }
        }
    }
    "nekobox_core".into()
}

impl App {
    pub fn new(args: &Args) -> Self {
        let config_dir = args
            .config_dir
            .clone()
            .map(PathBuf::from)
            .unwrap_or_else(ncore::store::get_base_path);

        let datastore = ncore::store::load_settings(&config_dir);
        let window = WindowSettings::load(&config_dir);
        let groups = Self::load_groups(&config_dir);
        let profiles = Self::load_profiles(&config_dir);
        let chains = Self::load_chains(&config_dir);

        // Restore the mode the GUI last persisted (`spmode2`).
        let spmode = if datastore.remember_spmode.iter().any(|s| s == "system_proxy") {
            SpMode::SystemProxy
        } else if datastore.remember_spmode.iter().any(|s| s == "vpn")
            || datastore.enable_tun_routing
        {
            SpMode::Tun
        } else {
            SpMode::Disabled
        };

        let worker = crate::rpc_worker::spawn();

        if args.launch {
            let cfg = nrpc::CoreConfig {
                binary: resolve_core_binary(args.core_binary.clone()),
                port: args.core_port.unwrap_or(datastore.core_port as u16),
                address: args.core_address.clone(),
                use_uds: args.core_uds_path.is_some(),
                uds_path: args
                    .core_uds_path
                    .clone()
                    .unwrap_or_else(|| "/tmp/nekobox_tui_core.sock".into()),
                ..Default::default()
            };
            let _ = worker.tx.send(Command::Launch {
                config: Box::new(cfg),
            });
        } else if let Some(path) = args.core_uds_path.clone() {
            let _ = worker.tx.send(Command::ConnectUds { path });
        } else {
            let _ = worker.tx.send(Command::Connect {
                address: args.core_address.clone(),
                port: args.core_port.unwrap_or(datastore.core_port as u16),
            });
        }

        // Follow the group the GUI left selected.
        let current_group = groups
            .iter()
            .position(|g| g.base.id == datastore.current_group)
            .unwrap_or(0);

        let mut app = Self {
            running: true,
            config_dir,
            datastore,
            window,
            groups,
            profiles,
            chains,
            current_group,
            selected: 0,
            table_state: TableState::default(),
            checked: HashSet::new(),
            latencies: HashMap::new(),
            core_status: CoreStatus::Connecting,
            active_profile_id: None,
            traffic_proxy: TrafficData::new("proxy"),
            traffic_direct: TrafficData::new("direct"),
            speed_history: VecDeque::with_capacity(SPEED_HISTORY_CAP),
            connections: Vec::new(),
            spmode,
            system_dns: false,
            bottom_tab: BottomTab::Logs,
            focus: Focus::Table,
            menu: None,
            menu_bar: Vec::new(),
            menu_bar_xranges: Vec::new(),
            dialog: None,
            settings_sel: 0,
            settings_edit: None,
            routes_sel: 0,
            groups_sel: 0,
            speed_test_running: false,
            table_rows_area: Rect::default(),
            group_tab_xranges: Vec::new(),
            bottom_tab_xranges: Vec::new(),
            control_row_hits: Vec::new(),
            last_click: None,
            logs: VecDeque::new(),
            log_cursor: 0,
            search_visible: false,
            filter_mode: false,
            filter: String::new(),
            transient: None,
            url_test_running: false,
            url_test_done: HashSet::new(),
            url_test_expected: 0,
            url_test_started: None,
            last_stats_poll: Instant::now(),
            last_stats_at: None,
            worker,
        };
        app.system_dns = app.datastore.system_dns_set;
        app.rebuild_menu_bar();
        app.log(format!("config dir: {}", app.config_dir.display()));
        app.log(format!(
            "loaded {} groups, {} profiles, {} routing profiles",
            app.groups.len(),
            app.profiles.len(),
            app.chains.len()
        ));
        app.seed_latencies();
        app
    }

    /// Seed the latency column from persisted test results (`yc` in .cfg).
    fn seed_latencies(&mut self) {
        self.latencies = self
            .profiles
            .values()
            .filter(|p| p.latency_int != 0)
            .map(|p| (p.id, p.latency_int))
            .collect();
    }

    // ------------------------------------------------------------------
    // Config loading
    // ------------------------------------------------------------------

    fn load_groups(base: &std::path::Path) -> Vec<Group> {
        let mut groups = Vec::new();
        if let Ok(files) = ncore::store::list_store_files(base, "groups") {
            for (id, path) in files {
                if let Ok(mut g) = ncore::store::load_group(&path) {
                    let extra_path = ncore::store::get_file_path(base, "subscriptions", id);
                    if extra_path.exists() {
                        g.extra = ncore::store::load_group_extra(&extra_path, id).ok();
                    }
                    if !g.archive {
                        groups.push(g);
                    }
                }
            }
        }
        groups
    }

    fn load_profiles(base: &std::path::Path) -> HashMap<i32, ProxyEntity> {
        let mut profiles = HashMap::new();
        if let Ok(files) = ncore::store::list_store_files(base, "profiles") {
            for (id, path) in files {
                if let Ok(mut p) = ncore::store::load_proxy_entity(&path) {
                    let bean_path = ncore::store::get_file_path(base, "beans", id);
                    if bean_path.exists() {
                        p.bean_cfg = ncore::store::load_bean_cfg(&bean_path).ok();
                    }
                    profiles.insert(id, p);
                }
            }
        }
        profiles
    }

    fn load_chains(base: &std::path::Path) -> Vec<RoutingChain> {
        let mut chains = Vec::new();
        if let Ok(files) = ncore::store::list_store_files(base, "route_profiles") {
            for (_, path) in files {
                if let Ok(c) = ncore::store::load_route_chain(&path) {
                    chains.push(c);
                }
            }
        }
        chains
    }

    fn reload(&mut self) {
        self.datastore = ncore::store::load_settings(&self.config_dir);
        self.window = WindowSettings::load(&self.config_dir);
        self.groups = Self::load_groups(&self.config_dir);
        self.profiles = Self::load_profiles(&self.config_dir);
        self.chains = Self::load_chains(&self.config_dir);
        self.current_group = self.current_group.min(self.groups.len().saturating_sub(1));
        self.selected = 0;
        self.checked.clear();
        self.seed_latencies();
        self.log(format!(
            "reloaded: {} groups, {} profiles",
            self.groups.len(),
            self.profiles.len()
        ));
    }

    /// Persist both settings files and the window INI.
    fn save_settings(&mut self, what: &str) {
        self.datastore.enable_tun_routing = self.spmode.is_tun();
        self.datastore.remember_spmode = match self.spmode {
            SpMode::SystemProxy => vec!["system_proxy".to_string()],
            SpMode::Tun => vec!["vpn".to_string()],
            SpMode::Disabled => Vec::new(),
        };
        self.datastore.system_dns_set = self.system_dns;
        if let Some(g) = self.groups.get(self.current_group) {
            self.datastore.current_group = g.base.id;
        }
        match ncore::store::save_settings(&self.config_dir, &self.datastore) {
            Ok(()) => self.log(format!("saved: {what}")),
            Err(e) => self.log(format!("save failed: {e:#}")),
        }
        if let Err(e) = self.window.save() {
            self.log(format!("window.ini save failed: {e:#}"));
        }
    }

    // ------------------------------------------------------------------
    // View helpers
    // ------------------------------------------------------------------

    /// Ids of the current group's profiles, in group order, after filtering.
    fn visible_ids(&self) -> Vec<i32> {
        let Some(group) = self.groups.get(self.current_group) else {
            return Vec::new();
        };
        let filter = self.filter.to_lowercase();
        group
            .profiles
            .iter()
            .filter(|id| self.profiles.contains_key(id))
            .filter(|id| {
                if filter.is_empty() {
                    return true;
                }
                let p = &self.profiles[id];
                p.name.to_lowercase().contains(&filter)
                    || p.server_address.to_lowercase().contains(&filter)
                    || p.display_core_type().to_lowercase().contains(&filter)
            })
            .copied()
            .collect()
    }

    /// The row under the cursor.
    fn cursor_id(&self) -> Option<i32> {
        self.visible_ids().get(self.selected).copied()
    }

    /// The profiles an action applies to: the checked set if any, else the
    /// cursor row. Mirrors the GUI's `get_now_selected_list`.
    fn target_ids(&self) -> Vec<i32> {
        if self.checked.is_empty() {
            self.cursor_id().into_iter().collect()
        } else {
            // Keep group order rather than HashSet order.
            self.visible_ids()
                .into_iter()
                .filter(|id| self.checked.contains(id))
                .collect()
        }
    }

    fn current_group_id(&self) -> Option<i32> {
        self.groups.get(self.current_group).map(|g| g.base.id)
    }

    fn current_group_name(&self) -> String {
        self.groups
            .get(self.current_group)
            .map(|g| g.name.clone())
            .unwrap_or_else(|| "(no groups)".into())
    }

    fn active_profile_name(&self) -> String {
        self.active_profile_id
            .and_then(|id| self.profiles.get(&id))
            .map(|p| p.display_type_and_name())
            .unwrap_or_default()
    }

    /// The routing chain the config builder should apply (`current_route_id`).
    fn active_chain(&self) -> Option<&RoutingChain> {
        self.chains
            .iter()
            .find(|c| c.base.id == self.datastore.current_route_id)
            .or_else(|| self.chains.first())
    }

    fn log(&mut self, msg: String) {
        if !self.window.logs_enabled {
            return;
        }
        // Collapse an immediately repeating line into a counter.
        if let Some(last) = self.logs.back_mut() {
            if last.text == msg {
                last.repeats = last.repeats.saturating_add(1);
                return;
            }
        }
        let is_error = ncore::log::is_error_line(&msg);
        while self.logs.len() >= self.window.max_log_line.max(1) {
            self.logs.pop_front();
        }
        self.logs.push_back(LogEntry {
            text: msg,
            is_error,
            repeats: 1,
        });
    }

    /// Show a message in the transient `data_view` line *and* the log.
    fn notify(&mut self, msg: impl Into<String>) {
        let msg = msg.into();
        self.transient = Some((msg.clone(), Instant::now()));
        self.log(msg);
    }

    /// Log lines after the errors-only filter.
    fn visible_logs(&self) -> Vec<&LogEntry> {
        self.logs
            .iter()
            .filter(|e| !self.window.errors_only || e.is_error)
            .collect()
    }

    // ------------------------------------------------------------------
    // Worker events & ticking
    // ------------------------------------------------------------------

    pub fn drain_worker_events(&mut self) {
        while let Ok(ev) = self.worker.rx.try_recv() {
            match ev {
                Event::Connected { privileged } => {
                    if matches!(self.core_status, CoreStatus::Connecting | CoreStatus::Error(_)) {
                        self.core_status = CoreStatus::Stopped;
                    }
                    self.log(format!("core connected (privileged: {privileged})"));
                    self.restore_last_profile();
                }
                Event::Started => self.core_status = CoreStatus::Running,
                Event::Stopped => {
                    self.core_status = CoreStatus::Stopped;
                    self.persist_active_traffic();
                    self.active_profile_id = None;
                    self.traffic_proxy = TrafficData::new("proxy");
                    self.traffic_direct = TrafficData::new("direct");
                    self.last_stats_at = None;
                    self.connections.clear();
                }
                Event::Stats { ups, downs } => self.apply_stats(&ups, &downs),
                Event::Connections(conns) => self.connections = conns,
                Event::SpeedTestDone {
                    tag,
                    dl_speed,
                    ul_speed,
                    latency,
                    country,
                    error,
                } => {
                    self.speed_test_running = false;
                    if error.is_empty() {
                        if let Ok(id) = tag.parse::<i32>() {
                            if latency > 0 {
                                self.latencies.insert(id, latency);
                            }
                            if let Some(p) = self.profiles.get_mut(&id) {
                                p.dl_speed = Some(dl_speed.clone());
                                p.ul_speed = Some(ul_speed.clone());
                                if !country.is_empty() {
                                    p.test_country = Some(country.clone());
                                }
                                let p = p.clone();
                                let _ = ncore::store::save_proxy_entity(&self.config_dir, &p);
                            }
                        }
                        let where_ = if country.is_empty() {
                            String::new()
                        } else {
                            format!(" [{country}]")
                        };
                        self.notify(format!(
                            "speed test: ▼{dl_speed} ▲{ul_speed} {latency} ms{where_}"
                        ));
                    } else {
                        self.notify(format!("speed test failed: {error}"));
                    }
                }
                Event::SubProfiles { gid, entities } => self.apply_subscription(gid, entities),
                Event::UrlTestResults(results) => {
                    for (tag, latency, error) in results {
                        if !self.url_test_done.insert(tag.clone()) {
                            continue;
                        }
                        if let Ok(id) = tag.parse::<i32>() {
                            let value = if error.is_empty() { latency } else { -1 };
                            self.latencies.insert(id, value);
                            if let Some(p) = self.profiles.get_mut(&id) {
                                p.latency_int = value;
                                let p = p.clone();
                                let _ = ncore::store::save_proxy_entity(&self.config_dir, &p);
                            }
                        }
                    }
                    if self.url_test_done.len() >= self.url_test_expected {
                        self.url_test_running = false;
                        self.notify("url test finished");
                    }
                }
                Event::Log(msg) => self.log(msg),
                Event::Error(msg) => {
                    if matches!(self.core_status, CoreStatus::Connecting) {
                        self.core_status = CoreStatus::Error(msg.clone());
                    }
                    self.log(format!("error: {msg}"));
                }
            }
        }
    }

    /// Restart the profile the GUI last had running (`remember_id`), if the
    /// "Remember last profile" setting is on.
    fn restore_last_profile(&mut self) {
        if !self.window.remember_last_profile || self.active_profile_id.is_some() {
            return;
        }
        let id = self.datastore.started_id;
        if id < 0 || !self.profiles.contains_key(&id) {
            return;
        }
        self.start_profile(id);
    }

    /// Fold one `QueryStats` sample into the session counters.
    ///
    /// The core's `TotalOutbound(tag)` swaps the counter to zero as it reads
    /// it, so `ups`/`downs` are the bytes moved since the previous poll, not
    /// running totals. `TrafficLooper::UpdateAll` in the GUI accumulates them
    /// and divides by the measured interval for the rate; so do we.
    fn apply_stats(&mut self, ups: &[(String, i64)], downs: &[(String, i64)]) {
        let now = Instant::now();
        let interval_ms = self
            .last_stats_at
            .map(|t| now.duration_since(t).as_millis() as i64)
            .unwrap_or(0);
        self.last_stats_at = Some(now);

        let sum = |map: &[(String, i64)], tag: &str| -> i64 {
            map.iter().filter(|(k, _)| k == tag).map(|(_, v)| *v).sum()
        };
        let (proxy_up, proxy_down) = (sum(ups, "proxy"), sum(downs, "proxy"));
        self.traffic_proxy.add_delta(proxy_up, proxy_down, interval_ms);
        self.traffic_direct
            .add_delta(sum(ups, "direct"), sum(downs, "direct"), interval_ms);

        // The running profile owns the "proxy" tag, so its lifetime counters
        // (the table's Traffic column) grow by the same delta. The GUI keeps
        // these on `ProxyEntity::traffic_data` and flushes them to disk when
        // the profile stops; `persist_active_traffic` does that here.
        if proxy_up != 0 || proxy_down != 0 {
            if let Some(p) = self
                .active_profile_id
                .and_then(|id| self.profiles.get_mut(&id))
            {
                p.traffic_ul += proxy_up;
                p.traffic_dl += proxy_down;
            }
        }

        if self.speed_history.len() >= SPEED_HISTORY_CAP {
            self.speed_history.pop_front();
        }
        self.speed_history.push_back((
            self.traffic_proxy.up_rate.max(0.0) as u64,
            self.traffic_proxy.down_rate.max(0.0) as u64,
        ));
    }

    /// Write the running profile's accumulated traffic back to its `.json`,
    /// mirroring the `profile->Save()` loop the GUI runs when stopping.
    fn persist_active_traffic(&mut self) {
        if let Some(p) = self
            .active_profile_id
            .and_then(|id| self.profiles.get(&id))
            .cloned()
        {
            if let Err(e) = ncore::store::save_proxy_entity(&self.config_dir, &p) {
                self.log(format!("failed to save traffic for profile {}: {e}", p.id));
            }
        }
    }

    /// Periodic work: stats polling, URL test polling, transient expiry.
    pub fn maybe_tick(&mut self) {
        let now = Instant::now();
        if self
            .transient
            .as_ref()
            .is_some_and(|(_, t)| now.duration_since(*t) > TRANSIENT_TTL)
        {
            self.transient = None;
        }
        if now.duration_since(self.last_stats_poll) < STATS_INTERVAL {
            return;
        }
        self.last_stats_poll = now;

        if self.core_status == CoreStatus::Running {
            if !self.datastore.disable_traffic_stats {
                let _ = self.worker.tx.send(Command::QueryStats);
            } else {
                // Nothing will refresh the rates, so don't leave the status
                // line frozen at whatever speed was showing when stats were
                // switched off.
                self.traffic_proxy.clear_rates();
                self.traffic_direct.clear_rates();
                self.last_stats_at = None;
            }
            // Listing connections needs the Clash API, which is only in the
            // config when connection statistics are on; polling it otherwise
            // just produces "no clash server found" once a second. The GUI
            // likewise only refreshes the connections tab while it is visible.
            if self.datastore.connection_statistics && self.bottom_tab == BottomTab::Connections {
                let _ = self.worker.tx.send(Command::ListConnections);
            }
        }

        if self.url_test_running {
            if self
                .url_test_started
                .map(|t| t.elapsed() > URL_TEST_TIMEOUT)
                .unwrap_or(false)
            {
                self.url_test_running = false;
                self.notify("url test timed out");
            } else {
                let _ = self.worker.tx.send(Command::QueryUrlTest);
            }
        }
    }

    // ------------------------------------------------------------------
    // Menu
    // ------------------------------------------------------------------

    fn rebuild_menu_bar(&mut self) {
        let groups: Vec<(String, bool)> = self
            .groups
            .iter()
            .enumerate()
            .map(|(i, g)| (g.name.clone(), i == self.current_group))
            .collect();
        let routes: Vec<(i32, String, bool)> = self
            .chains
            .iter()
            .map(|c| {
                (
                    c.base.id,
                    c.chain_name.clone(),
                    c.base.id == self.datastore.current_route_id,
                )
            })
            .collect();
        let targets = self.target_ids();
        let ctx = MenuContext {
            groups: &groups,
            routes: &routes,
            spmode_system_proxy: self.spmode.is_system_proxy(),
            spmode_tun: self.spmode.is_tun(),
            system_dns: self.system_dns,
            remember_last_profile: self.window.remember_last_profile,
            allow_lan: matches!(self.datastore.inbound_address.as_str(), "0.0.0.0" | "::"),
            search_visible: self.search_visible,
            has_selection: !targets.is_empty(),
            single_selection: targets.len() == 1,
            is_subscription_group: self
                .groups
                .get(self.current_group)
                .and_then(|g| g.extra.as_ref())
                .and_then(|e| e.url.as_ref())
                .is_some_and(|u| !u.is_empty()),
            running: self.core_status == CoreStatus::Running,
            testing: self.url_test_running || self.speed_test_running,
        };
        self.menu_bar = menu::build_menu_bar(&ctx);
    }

    fn open_menu(&mut self, root: usize) {
        self.rebuild_menu_bar();
        if let Some(m) = self.menu_bar.get(root) {
            self.menu = Some(MenuState::open(root, m.items.clone()));
        }
    }

    fn cycle_menu(&mut self, delta: isize) {
        let Some(state) = &self.menu else { return };
        let len = self.menu_bar.len() as isize;
        if len == 0 {
            return;
        }
        let next = (state.root as isize + delta).rem_euclid(len) as usize;
        self.open_menu(next);
    }

    fn handle_menu_key(&mut self, key: &event::KeyEvent) {
        let Some(state) = self.menu.as_mut() else {
            return;
        };
        match key.code {
            KeyCode::Esc => self.menu = None,
            KeyCode::Down | KeyCode::Char('j') => state.move_cursor(1),
            KeyCode::Up | KeyCode::Char('k') => state.move_cursor(-1),
            KeyCode::Right | KeyCode::Char('l') => {
                if !state.enter_submenu() {
                    self.cycle_menu(1);
                }
            }
            KeyCode::Left | KeyCode::Char('h') => {
                if !state.leave_submenu() {
                    self.cycle_menu(-1);
                }
            }
            KeyCode::Enter | KeyCode::Char(' ') => {
                if state.enter_submenu() {
                    return;
                }
                let action = state.current().and_then(|i| i.action.clone());
                self.menu = None;
                if let Some(action) = action {
                    self.dispatch(action);
                }
            }
            _ => {}
        }
    }

    // ------------------------------------------------------------------
    // Actions
    // ------------------------------------------------------------------

    fn dispatch(&mut self, action: Action) {
        match action {
            // --- Program ---
            Action::SpModeSystemProxy => self.set_spmode(SpMode::SystemProxy),
            Action::SpModeTun => self.set_spmode(SpMode::Tun),
            Action::SpModeDisabled => self.set_spmode(SpMode::Disabled),
            Action::ToggleSystemDns => {
                self.system_dns = !self.system_dns;
                let _ = self.worker.tx.send(Command::SetSystemDns {
                    enable: self.system_dns,
                });
                self.save_settings("system dns");
            }
            Action::ToggleRememberLastProfile => {
                self.window.remember_last_profile = !self.window.remember_last_profile;
                self.save_settings("remember last profile");
            }
            Action::ToggleAllowLan => {
                // The GUI treats "0.0.0.0"/"::" as LAN-enabled.
                let lan = matches!(self.datastore.inbound_address.as_str(), "0.0.0.0" | "::");
                self.datastore.inbound_address =
                    if lan { "127.0.0.1" } else { "0.0.0.0" }.to_string();
                self.save_settings("inbound address");
                self.restart_proxy();
            }
            Action::RestartProxy => self.restart_proxy(),
            Action::OpenConfigFolder => self.open_config_folder(),
            Action::Exit => self.quit(),

            // --- Preferences ---
            Action::ShowGroups => self.dialog = Some(Dialog::Groups),
            Action::ShowBasicSettings => self.dialog = Some(Dialog::Settings),
            Action::ShowRoutingSettings => self.dialog = Some(Dialog::Routes),

            // --- Profiles ---
            Action::SwitchGroup(i) => self.set_group(i),
            Action::CopyLink => self.copy_links(false),
            Action::CopyLinkNekoray => self.copy_links(true),
            Action::ExportConfig => self.export_config(),
            Action::UpdateProfileFromClipboard => self.update_profile_from_clipboard(),
            Action::UpdateProfileFromFile => {
                if let Some(id) = self.target_ids().first().copied() {
                    self.prompt(
                        "Update profile from file — path",
                        PromptAction::UpdateProfileFromFile(id),
                    );
                }
            }
            Action::UrlTestSelected => self.url_test(self.target_ids()),
            Action::ClearTestResultSelected => self.clear_test_results(self.target_ids()),
            Action::SpeedTestSelected => self.speed_test(SpeedTestMode::Full),
            Action::DownloadTestSelected => self.speed_test(SpeedTestMode::Download),
            Action::UploadTestSelected => self.speed_test(SpeedTestMode::Upload),
            Action::CountryTestSelected => self.speed_test(SpeedTestMode::Country),
            Action::SimpleDlSelected => self.speed_test(SpeedTestMode::SimpleDownload),
            Action::Start => {
                if let Some(id) = self.cursor_id() {
                    self.start_profile(id);
                }
            }
            Action::Stop => {
                // Drain the counters one last time before the core goes away,
                // like the GUI's final `UpdateAll()` on stop. The worker is a
                // single thread, so this sample is delivered before `Stopped`.
                if !self.datastore.disable_traffic_stats {
                    let _ = self.worker.tx.send(Command::QueryStats);
                }
                let _ = self.worker.tx.send(Command::Stop);
            }
            Action::SelectAll => {
                self.checked = self.visible_ids().into_iter().collect();
            }
            Action::UnselectAll => self.checked.clear(),
            Action::CloneProfile => self.clone_selected(),
            Action::MoveProfile => self.pick_move_target(),
            Action::DeleteProfile => self.ask_delete_selected(),
            Action::ResetTrafficSelected => self.reset_traffic(self.target_ids()),

            Action::AddFromClipboard => self.import_clipboard(),
            Action::AddProfileFromFile => {
                self.prompt("Add profile from file — path", PromptAction::AddProfileFromFile)
            }
            Action::AddNewGroup => self.prompt("New group name", PromptAction::NewGroup),
            Action::DeleteGroup => self.ask_delete_group(),
            Action::ToggleSearchBox => {
                self.search_visible = !self.search_visible;
                if !self.search_visible {
                    self.filter.clear();
                    self.filter_mode = false;
                }
            }

            // --- Group operations ---
            Action::UpdateSubscription => self.update_subscription(),
            Action::RemoveInvalid => self.remove_invalid(),
            Action::RemoveUnavailable => self.remove_unavailable(),
            Action::ClearTestResultGroup => self.clear_test_results(self.visible_ids()),
            Action::RemoveDuplicates => self.remove_duplicates(),
            Action::SpeedTestGroup => self.speed_test_group(),
            Action::ResetTrafficGroup => self.reset_traffic(self.visible_ids()),

            // --- Routing ---
            Action::SetRoute(id) => {
                self.datastore.current_route_id = id;
                let name = self
                    .chains
                    .iter()
                    .find(|c| c.base.id == id)
                    .map(|c| c.chain_name.clone())
                    .unwrap_or_default();
                self.datastore.active_routing = name.clone();
                self.save_settings(&format!("routing profile: {name}"));
                self.restart_proxy();
            }

            // --- Test ---
            Action::UrlTestGroup => self.url_test(self.visible_ids()),
            Action::SpeedTestCurrent => self.speed_test_current(),
            Action::StopTesting => {
                let _ = self.worker.tx.send(Command::StopTest);
                self.url_test_running = false;
                self.speed_test_running = false;
            }

            // --- Information ---
            Action::ShowStats => self.show_statistics(),
            Action::ShowAbout => self.show_about(),
        }
        self.rebuild_menu_bar();
    }

    fn set_spmode(&mut self, mode: SpMode) {
        if self.spmode == mode {
            return;
        }
        let was_system_proxy = self.spmode.is_system_proxy();
        self.spmode = mode;
        self.datastore.enable_tun_routing = mode.is_tun();

        // Tear the old system proxy down / bring the new one up.
        if was_system_proxy && !mode.is_system_proxy() {
            let _ = self.worker.tx.send(Command::SetSystemProxy {
                enable: false,
                address: self.datastore.inbound_address.clone(),
                port: self.datastore.inbound_socks_port,
            });
        }
        if mode.is_system_proxy() {
            let _ = self.worker.tx.send(Command::SetSystemProxy {
                enable: true,
                address: self.datastore.inbound_address.clone(),
                port: self.datastore.inbound_socks_port,
            });
        }
        self.save_settings(match mode {
            SpMode::Disabled => "special proxy: disabled",
            SpMode::SystemProxy => "special proxy: system proxy",
            SpMode::Tun => "special proxy: tun",
        });
        // TUN changes the inbound, so the core needs a new config.
        self.restart_proxy();
    }

    fn set_group(&mut self, index: usize) {
        if index >= self.groups.len() {
            return;
        }
        self.current_group = index;
        self.selected = 0;
        self.checked.clear();
        if let Some(g) = self.groups.get(index) {
            self.datastore.current_group = g.base.id;
        }
        let _ = ncore::store::save_datastore(&self.config_dir, &self.datastore);
    }

    /// Build and send the config for `id`.
    fn start_profile(&mut self, id: i32) {
        let Some(profile) = self.profiles.get(&id).cloned() else {
            return;
        };
        self.datastore.enable_tun_routing = self.spmode.is_tun();
        let chain = self.active_chain().cloned();
        match ncore::config::build_config_with_route(&profile, &self.datastore, chain.as_ref()) {
            Ok(config) => {
                self.notify(format!("starting: {}", profile.display_type_and_name()));
                self.active_profile_id = Some(id);
                self.core_status = CoreStatus::Connecting;
                self.datastore.started_id = id;
                let _ = ncore::store::save_datastore(&self.config_dir, &self.datastore);
                let _ = self.worker.tx.send(Command::Start {
                    config_json: config.to_string(),
                });
            }
            Err(e) => self.notify(format!("config build failed: {e:#}")),
        }
    }

    /// Re-send the current profile's config (after a setting that affects it).
    fn restart_proxy(&mut self) {
        if self.core_status != CoreStatus::Running {
            return;
        }
        if let Some(id) = self.active_profile_id {
            self.start_profile(id);
        }
    }

    fn open_config_folder(&mut self) {
        let dir = self.config_dir.clone();
        let opener = if cfg!(target_os = "windows") {
            "explorer"
        } else {
            "xdg-open"
        };
        match std::process::Command::new(opener).arg(&dir).spawn() {
            Ok(_) => self.notify(format!("opened {}", dir.display())),
            Err(e) => self.notify(format!("could not open config folder: {e}")),
        }
    }

    // --- profile operations ---

    /// Persist new profiles into a group and reload.
    fn add_profiles_to(&mut self, gid: i32, mut entities: Vec<ProxyEntity>) -> usize {
        let base = self.config_dir.clone();
        let start_id = ncore::store::next_store_id(&base, "profiles");
        let mut added = 0;
        for (offset, e) in entities.iter_mut().enumerate() {
            e.id = start_id + offset as i32;
            e.gid = gid;
            if let Err(err) = ncore::store::save_proxy_entity(&base, e) {
                self.log(format!("save profile failed: {err:#}"));
                continue;
            }
            if let Some(bean) = &e.bean_cfg {
                let _ = ncore::store::save_bean_cfg(&base, e.id, bean);
            }
            if let Some(g) = self.groups.iter_mut().find(|g| g.base.id == gid) {
                g.add_profile(e.id);
            }
            added += 1;
        }
        if let Some(g) = self.groups.iter().find(|g| g.base.id == gid) {
            let _ = ncore::store::save_group(&base, g);
        }
        self.reload();
        added
    }

    /// Persist new profiles into the current group and reload.
    fn add_profiles(&mut self, entities: Vec<ProxyEntity>) -> usize {
        let Some(gid) = self.current_group_id() else {
            self.notify("no group selected");
            return 0;
        };
        self.add_profiles_to(gid, entities)
    }

    fn import_clipboard(&mut self) {
        let text = match clipboard_get() {
            Ok(t) => t,
            Err(e) => {
                self.notify(format!("clipboard read failed: {e:#}"));
                return;
            }
        };
        match ncore::sub::parse_subscription(&text) {
            Ok(parsed) => {
                let entities: Vec<ProxyEntity> = parsed.into_iter().map(|p| p.entity).collect();
                let n = self.add_profiles(entities);
                self.notify(format!("imported {n} profile(s)"));
            }
            Err(e) => self.notify(format!("import failed: {e:#}")),
        }
    }

    fn import_file(&mut self, path: &str) {
        match std::fs::read_to_string(shellexpand(path)) {
            Ok(text) => match ncore::sub::parse_subscription(&text) {
                Ok(parsed) => {
                    let entities: Vec<ProxyEntity> = parsed.into_iter().map(|p| p.entity).collect();
                    let n = self.add_profiles(entities);
                    self.notify(format!("imported {n} profile(s)"));
                }
                Err(e) => self.notify(format!("import failed: {e:#}")),
            },
            Err(e) => self.notify(format!("read failed: {e}")),
        }
    }

    /// Replace one profile's settings from a share link, keeping its id/group.
    fn replace_profile(&mut self, id: i32, link: &str) {
        let parsed = match ncore::sub::parse_subscription(link) {
            Ok(p) => p,
            Err(e) => {
                self.notify(format!("parse failed: {e:#}"));
                return;
            }
        };
        let Some(first) = parsed.into_iter().next() else {
            self.notify("no profile found in input");
            return;
        };
        let Some(old) = self.profiles.get(&id) else {
            return;
        };
        let mut new = first.entity;
        new.id = id;
        new.gid = old.gid;
        new.traffic_dl = old.traffic_dl;
        new.traffic_ul = old.traffic_ul;
        if let Err(e) = ncore::store::save_proxy_entity(&self.config_dir, &new) {
            self.notify(format!("save failed: {e:#}"));
            return;
        }
        if let Some(bean) = &new.bean_cfg {
            let _ = ncore::store::save_bean_cfg(&self.config_dir, id, bean);
        }
        self.notify(format!("updated {}", new.display_type_and_name()));
        self.reload();
    }

    fn update_profile_from_clipboard(&mut self) {
        let Some(id) = self.target_ids().first().copied() else {
            return;
        };
        match clipboard_get() {
            Ok(text) => self.replace_profile(id, &text),
            Err(e) => self.notify(format!("clipboard read failed: {e:#}")),
        }
    }

    fn copy_links(&mut self, nekoray: bool) {
        let ids = self.target_ids();
        let mut links = Vec::new();
        for id in &ids {
            if let Some(p) = self.profiles.get(id) {
                match ncore::sub::to_share_link(p) {
                    Ok(link) => links.push(if nekoray {
                        // The GUI's "Nekoray link" is the share link wrapped
                        // in the nekoray scheme.
                        format!("nekoray://{}", base64_url(&link))
                    } else {
                        link
                    }),
                    Err(e) => self.log(format!("export failed for {}: {e:#}", p.name)),
                }
            }
        }
        if links.is_empty() {
            self.notify("nothing to copy");
            return;
        }
        let n = links.len();
        match clipboard_set(links.join("\n")) {
            Ok(()) => self.notify(format!("Copied {n} item(s)")),
            Err(e) => self.notify(format!("clipboard write failed: {e:#}")),
        }
    }

    fn export_config(&mut self) {
        let Some(id) = self.target_ids().first().copied() else {
            return;
        };
        let Some(profile) = self.profiles.get(&id).cloned() else {
            return;
        };
        let chain = self.active_chain().cloned();
        match ncore::config::build_config_with_route(&profile, &self.datastore, chain.as_ref()) {
            Ok(config) => {
                let body = serde_json::to_string_pretty(&config).unwrap_or_else(|_| config.to_string());
                let _ = clipboard_set(body.clone());
                self.dialog = Some(Dialog::Text {
                    title: format!("Config copied — {}", profile.display_type_and_name()),
                    body,
                    scroll: 0,
                });
            }
            Err(e) => self.notify(format!("config build failed: {e:#}")),
        }
    }

    fn clone_selected(&mut self) {
        let ids = self.target_ids();
        let mut clones = Vec::new();
        for id in ids {
            if let Some(p) = self.profiles.get(&id) {
                let mut c = p.clone();
                c.name = format!("{} copy", c.name);
                c.traffic_dl = 0;
                c.traffic_ul = 0;
                clones.push(c);
            }
        }
        let n = self.add_profiles(clones);
        self.notify(format!("cloned {n} profile(s)"));
    }

    fn ask_delete_selected(&mut self) {
        let ids = self.target_ids();
        if ids.is_empty() {
            return;
        }
        if !self.window.ask_delete {
            self.delete_profiles(ids);
            return;
        }
        let preview: Vec<String> = ids
            .iter()
            .take(10)
            .filter_map(|id| self.profiles.get(id))
            .map(|p| p.display_type_and_name())
            .collect();
        self.dialog = Some(Dialog::Confirm(Confirm {
            message: format!(
                "Remove {} item(s)?\n\n{}{}",
                ids.len(),
                preview.join("\n"),
                if ids.len() > 10 { "\n..." } else { "" }
            ),
            action: ConfirmAction::DeleteProfiles(ids),
        }));
    }

    fn delete_profiles(&mut self, ids: Vec<i32>) {
        for id in &ids {
            let _ = ncore::store::delete_profile_files(&self.config_dir, *id);
            if self.active_profile_id == Some(*id) {
                self.active_profile_id = None;
            }
        }
        for g in self.groups.iter_mut() {
            let before = g.profiles.len();
            g.profiles.retain(|p| !ids.contains(p));
            if g.profiles.len() != before {
                let _ = ncore::store::save_group(&self.config_dir, g);
            }
        }
        self.notify(format!("deleted {} profile(s)", ids.len()));
        self.reload();
    }

    fn reset_traffic(&mut self, ids: Vec<i32>) {
        for id in &ids {
            if let Some(p) = self.profiles.get_mut(id) {
                p.traffic_dl = 0;
                p.traffic_ul = 0;
                let p = p.clone();
                let _ = ncore::store::save_proxy_entity(&self.config_dir, &p);
            }
        }
        self.notify(format!("traffic reset for {} profile(s)", ids.len()));
    }

    fn clear_test_results(&mut self, ids: Vec<i32>) {
        for id in &ids {
            self.latencies.remove(id);
            if let Some(p) = self.profiles.get_mut(id) {
                p.latency_int = 0;
                p.dl_speed = None;
                p.ul_speed = None;
                p.full_test_report = None;
                p.test_country = None;
                let p = p.clone();
                let _ = ncore::store::save_proxy_entity(&self.config_dir, &p);
            }
        }
        self.notify(format!("cleared test results for {} profile(s)", ids.len()));
    }

    /// Remove profiles whose last URL test failed (`latencyInt < 0`).
    /// Port of `on_menu_remove_unavailable_triggered`.
    fn remove_unavailable(&mut self) {
        let ids: Vec<i32> = self
            .visible_ids()
            .into_iter()
            .filter(|id| self.latencies.get(id).is_some_and(|l| *l < 0))
            .collect();
        self.confirm_removal(ids, "Unavailable");
    }

    /// Remove profiles that cannot produce a valid outbound.
    /// Port of `on_menu_remove_invalid_triggered`.
    fn remove_invalid(&mut self) {
        let ids: Vec<i32> = self
            .visible_ids()
            .into_iter()
            .filter(|id| {
                self.profiles.get(id).is_none_or(|p| {
                    p.server_address.trim().is_empty()
                        || p.server_port <= 0
                        || ncore::config::build_outbound(p).get("type").is_none()
                })
            })
            .collect();
        self.confirm_removal(ids, "Invalid");
    }

    /// Remove later duplicates, keyed on (type, address, port, bean) the way
    /// `ProfileFilter::Uniq` does.
    fn remove_duplicates(&mut self) {
        let mut seen = HashSet::new();
        let mut dupes = Vec::new();
        for id in self.visible_ids() {
            let Some(p) = self.profiles.get(&id) else {
                continue;
            };
            if !seen.insert(Self::profile_key(p)) {
                dupes.push(id);
            }
        }
        self.confirm_removal(dupes, "Duplicate");
    }

    fn confirm_removal(&mut self, ids: Vec<i32>, what: &'static str) {
        if ids.is_empty() {
            self.notify(format!("no {} item(s) found", what.to_lowercase()));
            return;
        }
        if !self.window.ask_delete {
            self.delete_profiles(ids);
            return;
        }
        let preview: Vec<String> = ids
            .iter()
            .take(10)
            .filter_map(|id| self.profiles.get(id))
            .map(|p| p.display_type_and_name())
            .collect();
        self.dialog = Some(Dialog::Confirm(Confirm {
            message: format!(
                "Remove {} {what} item(s)?\n\n{}{}",
                ids.len(),
                preview.join("\n"),
                if ids.len() > 10 { "\n..." } else { "" }
            ),
            action: ConfirmAction::RemoveProfiles { ids, what },
        }));
    }

    fn pick_move_target(&mut self) {
        if self.target_ids().is_empty() {
            return;
        }
        let current = self.current_group_id();
        let items: Vec<(String, PickAction)> = self
            .groups
            .iter()
            .filter(|g| Some(g.base.id) != current)
            .map(|g| (g.name.clone(), PickAction::MoveToGroup(g.base.id)))
            .collect();
        if items.is_empty() {
            self.notify("no other group to move to");
            return;
        }
        self.dialog = Some(Dialog::Picker(Picker {
            title: "Move to group".into(),
            items,
            cursor: 0,
        }));
    }

    fn move_profiles(&mut self, ids: Vec<i32>, target_gid: i32) {
        for g in self.groups.iter_mut() {
            let before = g.profiles.len();
            if g.base.id == target_gid {
                for id in &ids {
                    g.add_profile(*id);
                }
            } else {
                g.profiles.retain(|p| !ids.contains(p));
            }
            if g.profiles.len() != before {
                let _ = ncore::store::save_group(&self.config_dir, g);
            }
        }
        for id in &ids {
            if let Some(p) = self.profiles.get_mut(id) {
                p.gid = target_gid;
                let p = p.clone();
                let _ = ncore::store::save_proxy_entity(&self.config_dir, &p);
            }
        }
        self.notify(format!("moved {} profile(s)", ids.len()));
        self.reload();
    }

    // --- group operations ---

    fn create_group(&mut self, name: String) {
        let mut g = Group::new();
        g.base.id = ncore::store::next_store_id(&self.config_dir, "groups");
        g.name = name.clone();
        match ncore::store::save_group(&self.config_dir, &g) {
            Ok(()) => {
                self.notify(format!("group created: {name}"));
                self.reload();
                if let Some(i) = self.groups.iter().position(|x| x.base.id == g.base.id) {
                    self.set_group(i);
                }
            }
            Err(e) => self.notify(format!("create group failed: {e:#}")),
        }
    }

    fn ask_delete_group(&mut self) {
        // The menu entry is "Delete current Group"; `groups_sel` belongs to the
        // Groups dialog and is stale here (the dialog has its own `d` binding).
        let Some(g) = self.groups.get(self.current_group) else {
            return;
        };
        let (id, name, count) = (g.base.id, g.name.clone(), g.profiles.len());
        self.dialog = Some(Dialog::Confirm(Confirm {
            message: format!("Delete group \"{name}\" and its {count} profile(s)?"),
            action: ConfirmAction::DeleteGroup(id),
        }));
    }

    fn delete_group(&mut self, gid: i32) {
        let ids: Vec<i32> = self
            .groups
            .iter()
            .find(|g| g.base.id == gid)
            .map(|g| g.profiles.clone())
            .unwrap_or_default();
        for id in ids {
            let _ = ncore::store::delete_profile_files(&self.config_dir, id);
        }
        let _ = ncore::store::delete_group_files(&self.config_dir, gid);
        self.notify("group deleted");
        self.current_group = 0;
        self.reload();
    }

    fn rename_group(&mut self, gid: i32, name: String) {
        if let Some(g) = self.groups.iter_mut().find(|g| g.base.id == gid) {
            g.name = name.clone();
            let g = g.clone();
            let _ = ncore::store::save_group(&self.config_dir, &g);
            self.notify(format!("group renamed: {name}"));
        }
    }

    fn set_group_url(&mut self, gid: i32, url: String) {
        if let Some(g) = self.groups.iter_mut().find(|g| g.base.id == gid) {
            let extra = g.extra.get_or_insert_with(Default::default);
            extra.id = gid;
            extra.url = Some(url.clone());
            let extra = extra.clone();
            g.is_subscription = !url.is_empty();
            let g = g.clone();
            let _ = ncore::store::save_group(&self.config_dir, &g);
            match ncore::store::save_group_extra(&self.config_dir, &extra) {
                Ok(()) => self.notify("subscription URL saved"),
                Err(e) => self.notify(format!("save failed: {e:#}")),
            }
        }
    }

    fn update_subscription(&mut self) {
        let Some((gid, name, url)) = self.groups.get(self.current_group).and_then(|g| {
            let url = g
                .extra
                .as_ref()
                .and_then(|e| e.url.clone())
                .filter(|u| !u.is_empty())?;
            Some((g.base.id, g.name.clone(), url))
        }) else {
            self.notify("current group has no subscription URL");
            return;
        };
        self.notify(format!("updating subscription: {name}"));
        let _ = self.worker.tx.send(Command::UpdateSubscription {
            gid,
            url,
            user_agent: self.datastore.user_agent.clone(),
        });
    }

    /// A key matching `ProfileFilterKey`: two profiles are "the same" when
    /// type, address, port and bean all match.
    fn profile_key(p: &ProxyEntity) -> (String, String, i32, String) {
        (
            p.r#type.clone(),
            p.server_address.clone(),
            p.server_port,
            p.serialize_bean(),
        )
    }

    /// Merge freshly fetched profiles into the group the update was started
    /// for (tracked by `gid` — the user may have switched groups since),
    /// applying the subscription post-processing flags the GUI honours.
    ///
    /// Like `GroupUpdater`, unchanged profiles (same key) keep their entries —
    /// with traffic counters and test results — while removed ones are deleted
    /// and new ones appended. A full wipe happens only with `sub_clear` (or
    /// over 1000 profiles, the GUI's own escape hatch).
    fn apply_subscription(&mut self, gid: i32, entities: Vec<ProxyEntity>) {
        let Some(g) = self.groups.iter().find(|x| x.base.id == gid).cloned() else {
            self.notify("subscription update: group no longer exists");
            return;
        };

        let mut entities = entities;
        if self.datastore.sub_rm_invalid {
            entities.retain(|e| !e.server_address.trim().is_empty() && e.server_port > 0);
        }
        if self.datastore.sub_rm_duplicates {
            let mut seen = HashSet::new();
            entities.retain(|e| {
                seen.insert(Self::profile_key(e))
            });
        }

        let clear = self.datastore.sub_clear || g.profiles.len() > 1000;
        let mut dropped: Vec<i32> = Vec::new();
        let mut kept: Vec<i32> = Vec::new();
        let mut fresh: Vec<ProxyEntity> = Vec::new();
        if clear {
            dropped = g.profiles.clone();
            fresh = entities;
        } else {
            let new_keys: HashSet<_> = entities.iter().map(Self::profile_key).collect();
            let mut kept_keys = HashSet::new();
            for id in &g.profiles {
                match self.profiles.get(id) {
                    Some(p) if new_keys.contains(&Self::profile_key(p)) => {
                        kept.push(*id);
                        kept_keys.insert(Self::profile_key(p));
                    }
                    _ => dropped.push(*id),
                }
            }
            for e in entities {
                if !kept_keys.contains(&Self::profile_key(&e)) {
                    fresh.push(e);
                }
            }
        }

        for id in &dropped {
            if self.active_profile_id == Some(*id) {
                self.notify("subscription removed the running profile; still running until restart");
            }
            let _ = ncore::store::delete_profile_files(&self.config_dir, *id);
            self.latencies.remove(id);
        }
        if let Some(g) = self.groups.iter_mut().find(|x| x.base.id == gid) {
            g.profiles = kept.clone();
            let g = g.clone();
            let _ = ncore::store::save_group(&self.config_dir, &g);
        }

        let added = self.add_profiles_to(gid, fresh);

        if let Some(g) = self.groups.iter().find(|x| x.base.id == gid) {
            if let Some(extra) = g.extra.as_ref() {
                let mut extra = extra.clone();
                extra.sub_last_update = Some(chrono_now());
                let _ = ncore::store::save_group_extra(&self.config_dir, &extra);
            }
        }
        self.notify(format!(
            "subscription updated: {added} new, {} kept, {} removed",
            kept.len(),
            dropped.len()
        ));
        if self.datastore.sub_url_test {
            let ids: Vec<i32> = self
                .groups
                .iter()
                .find(|x| x.base.id == gid)
                .map(|g| g.profiles.clone())
                .unwrap_or_default();
            self.url_test(ids);
        }
    }

    // --- tests ---

    fn url_test(&mut self, ids: Vec<i32>) {
        if self.url_test_running {
            self.notify("a url test is already running");
            return;
        }
        let profiles: Vec<&ProxyEntity> =
            ids.iter().filter_map(|id| self.profiles.get(id)).collect();
        if profiles.is_empty() {
            self.notify("url test: nothing to test");
            return;
        }
        let (config_json, tags) = ncore::config::build_test_config(&profiles);
        self.url_test_running = true;
        self.url_test_expected = tags.len();
        self.url_test_done.clear();
        self.url_test_started = Some(Instant::now());
        self.notify(format!("url test: {} profiles", tags.len()));
        let _ = self.worker.tx.send(Command::UrlTest {
            config_json,
            tags,
            url: self.datastore.test_latency_url.clone(),
            max_concurrency: self.datastore.test_concurrent,
            timeout_ms: self.datastore.url_test_timeout_ms,
        });
    }

    fn speed_test(&mut self, mode: SpeedTestMode) {
        if self.speed_test_running {
            self.notify("a speed test is already running");
            return;
        }
        let Some(id) = self.target_ids().first().copied() else {
            return;
        };
        let Some(p) = self.profiles.get(&id) else {
            return;
        };
        let name = p.display_type_and_name();
        let (config_json, tags) = ncore::config::build_test_config(&[p]);
        let Some(tag) = tags.into_iter().next() else {
            return;
        };
        self.speed_test_running = true;
        self.notify(format!("{}: {name}", mode.label()));
        let _ = self.worker.tx.send(Command::SpeedTest {
            config_json,
            tag,
            download_addr: self.datastore.simple_dl_url.clone(),
            timeout_ms: self.datastore.speed_test_timeout_ms,
            mode,
            test_current: false,
        });
    }

    /// "Speedtest Current" — test the outbound the core is actually running.
    fn speed_test_current(&mut self) {
        if self.core_status != CoreStatus::Running {
            self.notify("speedtest current: core is not running");
            return;
        }
        if self.speed_test_running {
            return;
        }
        self.speed_test_running = true;
        self.notify("speedtest current");
        let _ = self.worker.tx.send(Command::SpeedTest {
            config_json: String::new(),
            tag: "proxy".into(),
            download_addr: self.datastore.simple_dl_url.clone(),
            timeout_ms: self.datastore.speed_test_timeout_ms,
            mode: SpeedTestMode::Full,
            test_current: true,
        });
    }

    /// "Speedtest Group" — the GUI runs a full test across the group; the
    /// core takes one outbound at a time, so this queues the group's profiles
    /// through the URL test and then speed-tests the cursor row.
    fn speed_test_group(&mut self) {
        self.url_test(self.visible_ids());
    }

    fn show_statistics(&mut self) {
        let group_count = self.groups.len();
        let profile_count = self.profiles.len();
        let tested = self.latencies.len();
        let working = self.latencies.values().filter(|l| **l > 0).count();
        let total_dl: i64 = self.profiles.values().map(|p| p.traffic_dl).sum();
        let total_ul: i64 = self.profiles.values().map(|p| p.traffic_ul).sum();
        let body = format!(
            "Groups:              {group_count}\n\
             Profiles:            {profile_count}\n\
             Routing profiles:    {}\n\
             \n\
             Tested:              {tested}\n\
             Reachable:           {working}\n\
             Unavailable:         {}\n\
             \n\
             Lifetime download:   {}\n\
             Lifetime upload:     {}\n\
             \n\
             Session proxy down:  {}\n\
             Session proxy up:    {}\n\
             Session direct down: {}\n\
             Session direct up:   {}\n\
             \n\
             Config directory:    {}",
            self.chains.len(),
            tested - working,
            ncore::model::format_bytes(total_dl as u64, false),
            ncore::model::format_bytes(total_ul as u64, false),
            ncore::model::format_bytes(self.traffic_proxy.down as u64, false),
            ncore::model::format_bytes(self.traffic_proxy.up as u64, false),
            ncore::model::format_bytes(self.traffic_direct.down as u64, false),
            ncore::model::format_bytes(self.traffic_direct.up as u64, false),
            self.config_dir.display(),
        );
        self.dialog = Some(Dialog::Text {
            title: "Statistics".into(),
            body,
            scroll: 0,
        });
    }

    fn show_about(&mut self) {
        let body = format!(
            "nekobox-tui {}\n\n\
             Terminal front-end for NekoBox, sharing the Qt GUI's\n\
             configuration directory and nekobox_core RPC.\n\n\
             Licensed under GPL-3.0-only.\n\n\
             Config directory: {}",
            env!("CARGO_PKG_VERSION"),
            self.config_dir.display(),
        );
        self.dialog = Some(Dialog::Text {
            title: "About".into(),
            body,
            scroll: 0,
        });
    }

    fn prompt(&mut self, title: &str, action: PromptAction) {
        self.dialog = Some(Dialog::Prompt(Prompt {
            title: title.to_string(),
            buffer: String::new(),
            action,
        }));
    }

    /// Offer to route the domains mentioned by the highlighted log line.
    /// Port of the log context menu added upstream in 5.11.28.3.
    fn route_from_log(&mut self) {
        let logs = self.visible_logs();
        let Some(entry) = logs.get(self.log_cursor) else {
            return;
        };
        let domains = ncore::log::extract_domains(&entry.text);
        if domains.is_empty() {
            self.notify("no domain found in this log line");
            return;
        }
        let mut items = Vec::new();
        for domain in domains {
            for (label, action) in [
                ("Direct", ncore::model::SIMPLE_ACTION_DIRECT),
                ("Proxy", ncore::model::SIMPLE_ACTION_PROXY),
                ("Block", ncore::model::SIMPLE_ACTION_BLOCK),
            ] {
                items.push((
                    format!("{domain}  →  {label}"),
                    PickAction::RouteDomain {
                        domain: domain.clone(),
                        action,
                        match_type: "suffix",
                    },
                ));
            }
        }
        self.dialog = Some(Dialog::Picker(Picker {
            title: "Add domain to routing profile".into(),
            items,
            cursor: 0,
        }));
    }

    fn add_domain_to_route(&mut self, domain: &str, action: i32, match_type: &'static str) {
        let route_id = self.datastore.current_route_id;
        let Some(chain) = self.chains.iter_mut().find(|c| c.base.id == route_id) else {
            self.notify("No route profile selected. Open Routing Settings first.");
            return;
        };
        if !chain.add_domain_rule(domain, action, match_type) {
            self.notify(format!("{domain} is already routed"));
            return;
        }
        let chain = chain.clone();
        match ncore::store::save_route_chain(&self.config_dir, &chain) {
            Ok(()) => {
                let label = match action {
                    ncore::model::SIMPLE_ACTION_PROXY => "Proxy",
                    ncore::model::SIMPLE_ACTION_BLOCK => "Block",
                    _ => "Direct",
                };
                self.notify(format!("{domain} → {label}"));
                self.restart_proxy();
            }
            Err(e) => self.notify(format!("save failed: {e:#}")),
        }
    }

    // ------------------------------------------------------------------
    // Input
    // ------------------------------------------------------------------

    pub fn handle_key(&mut self, key: &event::KeyEvent) {
        // Menus and dialogs capture everything while open.
        if self.menu.is_some() {
            self.handle_menu_key(key);
            return;
        }
        if self.dialog.is_some() {
            self.handle_dialog_key(key);
            return;
        }
        if self.filter_mode {
            self.handle_filter_key(key);
            return;
        }

        // Global keys
        match key.code {
            KeyCode::Char('c') | KeyCode::Char('q')
                if key.modifiers.contains(KeyModifiers::CONTROL) =>
            {
                self.quit();
                return;
            }
            KeyCode::Char('q') => {
                self.quit();
                return;
            }
            KeyCode::Char('?') | KeyCode::F(1) => {
                self.dialog = Some(Dialog::Help);
                return;
            }
            // F2..F7 open the menu bar entries, mirroring the GUI's tool
            // buttons left to right.
            KeyCode::F(n) if (2..=7).contains(&n) => {
                self.open_menu(n as usize - 2);
                return;
            }
            // F10 activates the menu bar. Esc deliberately does not: it is
            // the cancel key everywhere else, and terminals also emit it as
            // the prefix of unrecognised escape sequences.
            KeyCode::F(10) => {
                self.open_menu(0);
                return;
            }
            KeyCode::Tab => {
                self.cycle_bottom_tab();
                return;
            }
            KeyCode::BackTab => {
                self.focus = match self.focus {
                    Focus::Table => Focus::Bottom,
                    Focus::Bottom => Focus::Table,
                };
                return;
            }
            _ => {}
        }

        match self.focus {
            Focus::Table => self.handle_table_key(key),
            Focus::Bottom => self.handle_bottom_key(key),
        }
    }

    fn handle_filter_key(&mut self, key: &event::KeyEvent) {
        match key.code {
            KeyCode::Esc => {
                self.filter_mode = false;
                self.filter.clear();
                self.selected = 0;
            }
            KeyCode::Enter => self.filter_mode = false,
            KeyCode::Backspace => {
                self.filter.pop();
                self.selected = 0;
            }
            KeyCode::Char(c) => {
                self.filter.push(c);
                self.selected = 0;
            }
            _ => {}
        }
    }

    fn handle_table_key(&mut self, key: &event::KeyEvent) {
        let count = self.visible_ids().len();
        match key.code {
            KeyCode::Char('j') | KeyCode::Down => {
                if count > 0 {
                    self.selected = (self.selected + 1).min(count - 1);
                }
            }
            KeyCode::Char('k') | KeyCode::Up => self.selected = self.selected.saturating_sub(1),
            KeyCode::Char('g') | KeyCode::Home => self.selected = 0,
            KeyCode::Char('G') | KeyCode::End => self.selected = count.saturating_sub(1),
            KeyCode::PageDown => self.selected = (self.selected + 10).min(count.saturating_sub(1)),
            KeyCode::PageUp => self.selected = self.selected.saturating_sub(10),
            // Space toggles the checkbox, matching the GUI's multi-selection.
            KeyCode::Char(' ') => {
                if let Some(id) = self.cursor_id() {
                    if !self.checked.remove(&id) {
                        self.checked.insert(id);
                    }
                    if count > 0 {
                        self.selected = (self.selected + 1).min(count - 1);
                    }
                }
            }
            KeyCode::Char('a') => self.dispatch(Action::SelectAll),
            KeyCode::Char('A') => self.dispatch(Action::UnselectAll),
            KeyCode::Enter => self.dispatch(Action::Start),
            KeyCode::Char('s') => self.dispatch(Action::Stop),
            KeyCode::Char('t') => self.dispatch(Action::UrlTestSelected),
            KeyCode::Char('T') => self.dispatch(Action::SpeedTestSelected),
            KeyCode::Char('u') => self.dispatch(if self.spmode.is_tun() {
                Action::SpModeDisabled
            } else {
                Action::SpModeTun
            }),
            KeyCode::Char('x') => self.dispatch(if self.spmode.is_system_proxy() {
                Action::SpModeDisabled
            } else {
                Action::SpModeSystemProxy
            }),
            KeyCode::Char('y') => self.dispatch(Action::CopyLink),
            KeyCode::Char('p') => self.dispatch(Action::AddFromClipboard),
            KeyCode::Char('d') => self.dispatch(Action::DeleteProfile),
            KeyCode::Char('c') => self.dispatch(Action::CloneProfile),
            KeyCode::Char('m') => self.dispatch(Action::MoveProfile),
            KeyCode::Char('U') => self.dispatch(Action::UpdateSubscription),
            KeyCode::Char('r') => self.reload(),
            KeyCode::Char('[') => self.switch_group(false),
            KeyCode::Char(']') => self.switch_group(true),
            KeyCode::Char('/') => {
                self.search_visible = true;
                self.filter_mode = true;
            }
            _ => {}
        }
    }

    fn handle_bottom_key(&mut self, key: &event::KeyEvent) {
        let len = self.visible_logs().len();
        match self.bottom_tab {
            BottomTab::Logs => match key.code {
                KeyCode::Char('j') | KeyCode::Down => {
                    if len > 0 {
                        self.log_cursor = (self.log_cursor + 1).min(len - 1);
                    }
                }
                KeyCode::Char('k') | KeyCode::Up => {
                    self.log_cursor = self.log_cursor.saturating_sub(1)
                }
                KeyCode::Char('g') | KeyCode::Home => self.log_cursor = 0,
                KeyCode::Char('G') | KeyCode::End => self.log_cursor = len.saturating_sub(1),
                // Upstream 5.11.28.3: errors-only filter + quick route add.
                KeyCode::Char('e') => {
                    self.window.errors_only = !self.window.errors_only;
                    self.log_cursor = 0;
                    let _ = self.window.save();
                }
                KeyCode::Char('a') => {
                    self.window.auto_scroll_log = !self.window.auto_scroll_log;
                    let _ = self.window.save();
                }
                KeyCode::Char('R') | KeyCode::Enter => self.route_from_log(),
                KeyCode::Char('c') => {
                    self.logs.clear();
                    self.log_cursor = 0;
                }
                KeyCode::Char('y') => {
                    let line = self
                        .visible_logs()
                        .get(self.log_cursor)
                        .map(|e| e.text.clone());
                    if let Some(line) = line {
                        let _ = clipboard_set(line);
                        self.notify("log line copied");
                    }
                }
                _ => {}
            },
            _ => {
                // Connections / graph: no per-row actions yet, but keep the
                // table keys working so focus is not a dead end.
                if matches!(key.code, KeyCode::Char('j') | KeyCode::Char('k')) {
                    self.handle_table_key(key);
                }
            }
        }
    }

    fn handle_dialog_key(&mut self, key: &event::KeyEvent) {
        // Settings inline edit captures input first.
        if self.settings_edit.is_some() && matches!(self.dialog, Some(Dialog::Settings)) {
            self.handle_settings_edit_key(key);
            return;
        }
        let Some(dialog) = self.dialog.as_mut() else {
            return;
        };
        match dialog {
            Dialog::Help => {
                if matches!(key.code, KeyCode::Esc | KeyCode::Char('q') | KeyCode::Char('?')) {
                    self.dialog = None;
                }
            }
            Dialog::Text { scroll, .. } => match key.code {
                KeyCode::Esc | KeyCode::Char('q') => self.dialog = None,
                KeyCode::Down | KeyCode::Char('j') => *scroll = scroll.saturating_add(1),
                KeyCode::Up | KeyCode::Char('k') => *scroll = scroll.saturating_sub(1),
                KeyCode::PageDown => *scroll = scroll.saturating_add(10),
                KeyCode::PageUp => *scroll = scroll.saturating_sub(10),
                _ => {}
            },
            Dialog::Confirm(_) => match key.code {
                KeyCode::Char('y') | KeyCode::Char('Y') | KeyCode::Enter => {
                    let Some(Dialog::Confirm(confirm)) = self.dialog.take() else {
                        return;
                    };
                    match confirm.action {
                        ConfirmAction::DeleteProfiles(ids) => self.delete_profiles(ids),
                        ConfirmAction::RemoveProfiles { ids, what } => {
                            let n = ids.len();
                            self.delete_profiles(ids);
                            self.notify(format!("removed {n} {} item(s)", what.to_lowercase()));
                        }
                        ConfirmAction::DeleteGroup(gid) => self.delete_group(gid),
                    }
                }
                _ => self.dialog = None,
            },
            Dialog::Prompt(prompt) => match key.code {
                KeyCode::Esc => self.dialog = None,
                KeyCode::Backspace => {
                    prompt.buffer.pop();
                }
                KeyCode::Char(c) => prompt.buffer.push(c),
                KeyCode::Enter => {
                    let Some(Dialog::Prompt(prompt)) = self.dialog.take() else {
                        return;
                    };
                    let value = prompt.buffer.trim().to_string();
                    if value.is_empty() {
                        return;
                    }
                    match prompt.action {
                        PromptAction::NewGroup => self.create_group(value),
                        PromptAction::RenameGroup(gid) => self.rename_group(gid, value),
                        PromptAction::SetGroupUrl(gid) => self.set_group_url(gid, value),
                        PromptAction::AddProfileFromFile => self.import_file(&value),
                        PromptAction::UpdateProfileFromFile(id) => {
                            match std::fs::read_to_string(shellexpand(&value)) {
                                Ok(text) => self.replace_profile(id, &text),
                                Err(e) => self.notify(format!("read failed: {e}")),
                            }
                        }
                    }
                }
                _ => {}
            },
            Dialog::Picker(picker) => match key.code {
                KeyCode::Esc | KeyCode::Char('q') => self.dialog = None,
                KeyCode::Down | KeyCode::Char('j') => {
                    if !picker.items.is_empty() {
                        picker.cursor = (picker.cursor + 1).min(picker.items.len() - 1);
                    }
                }
                KeyCode::Up | KeyCode::Char('k') => {
                    picker.cursor = picker.cursor.saturating_sub(1)
                }
                KeyCode::Enter => {
                    let Some(Dialog::Picker(mut picker)) = self.dialog.take() else {
                        return;
                    };
                    if picker.cursor < picker.items.len() {
                        match picker.items.remove(picker.cursor).1 {
                            PickAction::MoveToGroup(gid) => {
                                self.move_profiles(self.target_ids(), gid)
                            }
                            PickAction::RouteDomain {
                                domain,
                                action,
                                match_type,
                            } => self.add_domain_to_route(&domain, action, match_type),
                        }
                    }
                }
                _ => {}
            },
            Dialog::Settings => self.handle_settings_key(key),
            Dialog::Routes => self.handle_routes_key(key),
            Dialog::Groups => self.handle_groups_key(key),
        }
    }

    fn handle_routes_key(&mut self, key: &event::KeyEvent) {
        match key.code {
            KeyCode::Esc | KeyCode::Char('q') => self.dialog = None,
            KeyCode::Char('j') | KeyCode::Down => {
                if !self.chains.is_empty() {
                    self.routes_sel = (self.routes_sel + 1).min(self.chains.len() - 1);
                }
            }
            KeyCode::Char('k') | KeyCode::Up => self.routes_sel = self.routes_sel.saturating_sub(1),
            KeyCode::Enter => {
                if let Some(id) = self.chains.get(self.routes_sel).map(|c| c.base.id) {
                    self.dispatch(Action::SetRoute(id));
                }
            }
            _ => {}
        }
    }

    fn handle_groups_key(&mut self, key: &event::KeyEvent) {
        match key.code {
            KeyCode::Esc | KeyCode::Char('q') => self.dialog = None,
            KeyCode::Char('j') | KeyCode::Down => {
                if !self.groups.is_empty() {
                    self.groups_sel = (self.groups_sel + 1).min(self.groups.len() - 1);
                }
            }
            KeyCode::Char('k') | KeyCode::Up => self.groups_sel = self.groups_sel.saturating_sub(1),
            KeyCode::Enter => {
                let sel = self.groups_sel;
                self.set_group(sel);
                self.dialog = None;
            }
            KeyCode::Char('n') => self.prompt("New group name", PromptAction::NewGroup),
            KeyCode::Char('r') => {
                if let Some(gid) = self.groups.get(self.groups_sel).map(|g| g.base.id) {
                    self.prompt("Rename group", PromptAction::RenameGroup(gid));
                }
            }
            KeyCode::Char('u') => {
                if let Some(gid) = self.groups.get(self.groups_sel).map(|g| g.base.id) {
                    self.prompt("Subscription URL", PromptAction::SetGroupUrl(gid));
                }
            }
            KeyCode::Char('d') => {
                if let Some(g) = self.groups.get(self.groups_sel) {
                    let (id, name, count) = (g.base.id, g.name.clone(), g.profiles.len());
                    self.dialog = Some(Dialog::Confirm(Confirm {
                        message: format!("Delete group \"{name}\" and its {count} profile(s)?"),
                        action: ConfirmAction::DeleteGroup(id),
                    }));
                }
            }
            _ => {}
        }
    }

    fn handle_settings_key(&mut self, key: &event::KeyEvent) {
        match key.code {
            KeyCode::Esc | KeyCode::Char('q') => self.dialog = None,
            KeyCode::Char('j') | KeyCode::Down => {
                self.settings_sel = (self.settings_sel + 1).min(SETTINGS.len() - 1)
            }
            KeyCode::Char('k') | KeyCode::Up => {
                self.settings_sel = self.settings_sel.saturating_sub(1)
            }
            KeyCode::Enter | KeyCode::Char(' ') => {
                let def = &SETTINGS[self.settings_sel];
                match def.kind {
                    SettingKind::Bool => {
                        let current = (def.get)(&self.datastore) == "true";
                        (def.set)(&mut self.datastore, if current { "false" } else { "true" });
                        let label = def.label;
                        self.save_settings(label);
                    }
                    _ => self.settings_edit = Some((def.get)(&self.datastore)),
                }
            }
            _ => {}
        }
    }

    fn handle_settings_edit_key(&mut self, key: &event::KeyEvent) {
        match key.code {
            KeyCode::Esc => self.settings_edit = None,
            KeyCode::Enter => {
                if let Some(buf) = self.settings_edit.take() {
                    let def = &SETTINGS[self.settings_sel];
                    (def.set)(&mut self.datastore, &buf);
                    let label = def.label;
                    self.save_settings(label);
                }
            }
            KeyCode::Backspace => {
                if let Some(buf) = self.settings_edit.as_mut() {
                    buf.pop();
                }
            }
            KeyCode::Char(c) => {
                if let Some(buf) = self.settings_edit.as_mut() {
                    buf.push(c);
                }
            }
            _ => {}
        }
    }

    fn cycle_bottom_tab(&mut self) {
        let current = BottomTab::ALL
            .iter()
            .position(|t| *t == self.bottom_tab)
            .unwrap_or(0);
        self.bottom_tab = BottomTab::ALL[(current + 1) % BottomTab::ALL.len()];
    }

    fn switch_group(&mut self, forward: bool) {
        if self.groups.is_empty() {
            return;
        }
        let len = self.groups.len();
        let next = if forward {
            (self.current_group + 1) % len
        } else {
            (self.current_group + len - 1) % len
        };
        self.set_group(next);
    }

    fn quit(&mut self) {
        // Traffic accumulated this session only lives in memory until the
        // profile stops; flush it so quitting while running does not lose it.
        if self.core_status == CoreStatus::Running {
            self.persist_active_traffic();
        }
        let _ = self.worker.tx.send(Command::Shutdown);
        self.running = false;
    }

    // ------------------------------------------------------------------
    // Mouse
    // ------------------------------------------------------------------

    pub fn handle_mouse(&mut self, me: &event::MouseEvent) {
        use event::{MouseButton, MouseEventKind};
        if self.dialog.is_some() {
            return;
        }
        match me.kind {
            MouseEventKind::Down(MouseButton::Left) => {
                // Menu bar (row 0).
                if me.row == 0 {
                    for (i, (x0, x1)) in self.menu_bar_xranges.clone().iter().enumerate() {
                        if me.column >= *x0 && me.column < *x1 {
                            if self.menu.as_ref().map(|m| m.root) == Some(i) {
                                self.menu = None;
                            } else {
                                self.open_menu(i);
                            }
                            return;
                        }
                    }
                    self.menu = None;
                    return;
                }
                if self.menu.is_some() {
                    self.menu = None;
                    return;
                }
                // Control row checkboxes / buttons (row 1).
                for (x0, x1, action) in self.control_row_hits.clone() {
                    if me.row == 1 && me.column >= x0 && me.column < x1 {
                        self.dispatch(action);
                        return;
                    }
                }
                // Group tabs (row 2).
                if me.row == 2 {
                    for (i, (x0, x1)) in self.group_tab_xranges.clone().iter().enumerate() {
                        if me.column >= *x0 && me.column < *x1 && i < self.groups.len() {
                            self.set_group(i);
                            return;
                        }
                    }
                }
                // Bottom panel tabs.
                for (i, (x0, x1, row)) in self.bottom_tab_xranges.clone().iter().enumerate() {
                    if me.row == *row && me.column >= *x0 && me.column < *x1 {
                        self.bottom_tab = BottomTab::ALL[i];
                        self.focus = Focus::Bottom;
                        return;
                    }
                }
                // Profile rows.
                let area = self.table_rows_area;
                if me.column >= area.x
                    && me.column < area.x + area.width
                    && me.row >= area.y
                    && me.row < area.y + area.height
                {
                    let idx = (me.row - area.y) as usize;
                    let now = Instant::now();
                    let double = self
                        .last_click
                        .map(|(t, _, r)| r == me.row && now.duration_since(t).as_millis() < 500)
                        .unwrap_or(false);
                    self.focus = Focus::Table;
                    self.selected = idx;
                    self.last_click = Some((now, me.column, me.row));
                    if double {
                        self.dispatch(Action::Start);
                        self.last_click = None;
                    }
                }
            }
            MouseEventKind::ScrollUp => {
                let area = self.table_rows_area;
                if me.row >= area.y && me.row < area.y + area.height + 4 {
                    self.selected = self.selected.saturating_sub(1);
                }
            }
            MouseEventKind::ScrollDown => {
                let area = self.table_rows_area;
                if me.row >= area.y && me.row < area.y + area.height + 4 {
                    let count = self.visible_ids().len();
                    if count > 0 {
                        self.selected = (self.selected + 1).min(count - 1);
                    }
                }
            }
            _ => {}
        }
    }
}

// ============================================================================
// Small helpers
// ============================================================================

fn clipboard_get() -> Result<String, arboard::Error> {
    arboard::Clipboard::new().and_then(|mut c| c.get_text())
}

fn clipboard_set(text: String) -> Result<(), arboard::Error> {
    arboard::Clipboard::new().and_then(|mut c| c.set_text(text))
}

/// Expand a leading `~` so typed paths behave like they do in a shell.
fn shellexpand(path: &str) -> PathBuf {
    if let Some(rest) = path.strip_prefix("~/") {
        if let Some(home) = std::env::var_os("HOME") {
            return PathBuf::from(home).join(rest);
        }
    }
    PathBuf::from(path)
}

/// URL-safe base64 without padding, as used by `nekoray://` links.
fn base64_url(s: &str) -> String {
    use base64::Engine;
    base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(s)
}

fn chrono_now() -> i64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.as_secs() as i64)
        .unwrap_or(0)
}

// ============================================================================
// Settings dialog field descriptors
// ============================================================================

#[derive(Clone, Copy)]
enum SettingKind {
    Bool,
    Int,
    Str,
}

struct SettingDef {
    label: &'static str,
    kind: SettingKind,
    get: fn(&DataStore) -> String,
    set: fn(&mut DataStore, &str),
}

macro_rules! sdef {
    ($label:expr, bool, $field:ident) => {
        SettingDef {
            label: $label,
            kind: SettingKind::Bool,
            get: |ds| ds.$field.to_string(),
            set: |ds, v| ds.$field = v.trim() == "true" || v.trim() == "1",
        }
    };
    ($label:expr, int, $field:ident) => {
        SettingDef {
            label: $label,
            kind: SettingKind::Int,
            get: |ds| ds.$field.to_string(),
            set: |ds, v| {
                if let Ok(x) = v.trim().parse() {
                    ds.$field = x;
                }
            },
        }
    };
    ($label:expr, str, $field:ident) => {
        SettingDef {
            label: $label,
            kind: SettingKind::Str,
            get: |ds| ds.$field.clone(),
            set: |ds, v| ds.$field = v.trim().to_string(),
        }
    };
}

const SETTINGS: &[SettingDef] = &[
    sdef!("inbound_address", str, inbound_address),
    sdef!("inbound_socks_port", int, inbound_socks_port),
    sdef!("test_url", str, test_latency_url),
    sdef!("urltest_timeout_ms", int, url_test_timeout_ms),
    sdef!("speedtest_timeout_ms", int, speed_test_timeout_ms),
    sdef!("test_concurrent", int, test_concurrent),
    sdef!("simple_dl_url", str, simple_dl_url),
    sdef!("log_level", str, log_level),
    sdef!("mux_protocol", str, mux_protocol),
    sdef!("mux_concurrency", int, mux_concurrency),
    sdef!("remote_dns", str, remote_dns),
    sdef!("direct_dns", str, direct_dns),
    sdef!("use_dns_object", bool, use_dns_object),
    sdef!("domain_strategy", str, domain_strategy),
    sdef!("outbound_domain_strategy", str, outbound_domain_strategy),
    sdef!("sniffing_mode", int, sniffing_mode),
    sdef!("adblock_enable", bool, adblock_enable),
    sdef!("tun_address", str, tun_address),
    sdef!("tun_address_6", str, tun_address_6),
    sdef!("vpn_mtu", int, vpn_mtu),
    sdef!("vpn_ipv6", bool, vpn_ipv6),
    sdef!("vpn_strict_route", bool, vpn_strict_route),
    sdef!("vpn_implementation", str, vpn_implementation),
    sdef!("network_use_proxy", bool, network_use_proxy),
    sdef!("skip_cert", bool, skip_cert),
    sdef!("enable_tun_routing", bool, enable_tun_routing),
    sdef!("sub_rm_invalid", bool, sub_rm_invalid),
    sdef!("sub_rm_duplicates", bool, sub_rm_duplicates),
    sdef!("sub_url_test", bool, sub_url_test),
    sdef!("core_use_uds", bool, core_use_uds),
];

// ============================================================================
// Rendering
// ============================================================================

const ACCENT: Color = Color::Cyan;

pub fn render(frame: &mut Frame<'_>, app: &mut App) {
    // menubar / control row / group tabs / splitter / status labels
    let [menubar, control, tabs, content, status] = Layout::vertical([
        Constraint::Length(1),
        Constraint::Length(1),
        Constraint::Length(1),
        Constraint::Min(6),
        Constraint::Length(3),
    ])
    .areas(frame.area());

    render_menu_bar(frame, menubar, app);
    render_control_row(frame, control, app);
    render_group_tabs(frame, tabs, app);

    let bottom_height = (content.height / 3).clamp(6, 14);
    let [table_area, bottom_area] =
        Layout::vertical([Constraint::Min(4), Constraint::Length(bottom_height)]).areas(content);
    render_proxy_table(frame, table_area, app);
    render_bottom_panel(frame, bottom_area, app);

    render_status_bar(frame, status, app);

    if app.filter_mode {
        frame.set_cursor_position((
            control.x + control.width.saturating_sub(24) + 8 + app.filter.chars().count() as u16,
            control.y,
        ));
    }
    if app.menu.is_some() {
        render_menu_overlay(frame, app);
    }
    if app.dialog.is_some() {
        render_dialog(frame, app);
    }
}

fn render_menu_bar(frame: &mut Frame<'_>, area: Rect, app: &mut App) {
    if app.menu_bar.is_empty() {
        app.rebuild_menu_bar();
    }
    let open_root = app.menu.as_ref().map(|m| m.root);
    let mut spans = vec![Span::raw(" ")];
    let mut x = area.x + 1;
    app.menu_bar_xranges.clear();
    for (i, m) in app.menu_bar.iter().enumerate() {
        let label = format!(" {} ", m.title);
        let width = label.chars().count() as u16;
        let style = if open_root == Some(i) {
            Style::default().fg(Color::Black).bg(ACCENT).bold()
        } else {
            Style::default()
        };
        spans.push(Span::styled(label, style));
        app.menu_bar_xranges.push((x, x + width));
        x += width;
    }
    spans.push(Span::styled(
        "   F2..F7 menus · ? help",
        Style::default().fg(Color::DarkGray),
    ));
    frame.render_widget(
        Paragraph::new(Line::from(spans)).style(Style::default().bg(Color::Indexed(236))),
        area,
    );
}

/// The GUI's toolbar row: start/stop button, the three checkboxes, the
/// search box and the URL-test button.
fn render_control_row(frame: &mut Frame<'_>, area: Rect, app: &mut App) {
    app.control_row_hits.clear();
    let mut spans = Vec::new();
    let mut x = area.x + 1;
    spans.push(Span::raw(" "));

    let running = app.core_status == CoreStatus::Running;
    let (btn_label, btn_style, btn_action) = if running {
        (
            " ■ Stop ",
            Style::default().fg(Color::Black).bg(Color::LightRed).bold(),
            Action::Stop,
        )
    } else {
        (
            " ▶ Start ",
            Style::default().fg(Color::Black).bg(Color::LightGreen).bold(),
            Action::Start,
        )
    };
    spans.push(Span::styled(btn_label, btn_style));
    app.control_row_hits
        .push((x, x + btn_label.chars().count() as u16, btn_action));
    x += btn_label.chars().count() as u16;

    let checkbox = |spans: &mut Vec<Span<'static>>,
                        x: &mut u16,
                        label: &str,
                        checked: bool,
                        action: Action,
                        hits: &mut Vec<(u16, u16, Action)>| {
        let text = format!("  [{}] {}", if checked { "x" } else { " " }, label);
        let width = text.chars().count() as u16;
        spans.push(Span::styled(
            text,
            if checked {
                Style::default().fg(ACCENT).bold()
            } else {
                Style::default().fg(Color::Gray)
            },
        ));
        hits.push((*x, *x + width, action));
        *x += width;
    };

    let (tun, sysproxy, dns) = (
        app.spmode.is_tun(),
        app.spmode.is_system_proxy(),
        app.system_dns,
    );
    let mut hits = std::mem::take(&mut app.control_row_hits);
    checkbox(
        &mut spans,
        &mut x,
        "Tun",
        tun,
        if tun { Action::SpModeDisabled } else { Action::SpModeTun },
        &mut hits,
    );
    checkbox(
        &mut spans,
        &mut x,
        "System Proxy",
        sysproxy,
        if sysproxy {
            Action::SpModeDisabled
        } else {
            Action::SpModeSystemProxy
        },
        &mut hits,
    );
    checkbox(
        &mut spans,
        &mut x,
        "System DNS",
        dns,
        Action::ToggleSystemDns,
        &mut hits,
    );
    app.control_row_hits = hits;

    // Right side: either the search box or the transient `data_view` text.
    let used: u16 = spans.iter().map(|s| s.content.chars().count() as u16).sum();
    let right_width = area.width.saturating_sub(used).saturating_sub(1);
    if app.search_visible {
        let label = format!(
            "  Search: {}{}",
            app.filter,
            if app.filter_mode { "▌" } else { "" }
        );
        spans.push(Span::styled(label, Style::default().fg(ACCENT)));
    } else if let Some((msg, _)) = &app.transient {
        let mut msg = msg.clone();
        let budget = right_width.saturating_sub(2) as usize;
        if msg.chars().count() > budget {
            msg = msg.chars().take(budget).collect();
        }
        spans.push(Span::styled(
            format!("  {msg}"),
            Style::default().fg(Color::Yellow),
        ));
    }

    frame.render_widget(Paragraph::new(Line::from(spans)), area);
}

fn render_group_tabs(frame: &mut Frame<'_>, area: Rect, app: &mut App) {
    let mut spans = vec![Span::raw(" ")];
    let mut x = area.x + 1;
    app.group_tab_xranges.clear();
    if app.groups.is_empty() {
        spans.push(Span::styled(
            " (no groups — Profiles ▸ Add new Group) ",
            Style::default().fg(Color::DarkGray),
        ));
    }
    for (i, g) in app.groups.iter().enumerate() {
        let label = format!(" {} ({}) ", g.name, g.profiles.len());
        let width = label.chars().count() as u16;
        let style = if i == app.current_group {
            Style::default().fg(Color::Black).bg(ACCENT).bold()
        } else {
            Style::default().fg(Color::DarkGray)
        };
        spans.push(Span::styled(label, style));
        spans.push(Span::raw(" "));
        app.group_tab_xranges.push((x, x + width));
        x += width + 1;
    }
    frame.render_widget(Paragraph::new(Line::from(spans)), area);
}

fn render_proxy_table(frame: &mut Frame<'_>, area: Rect, app: &mut App) {
    let ids = app.visible_ids();
    app.selected = if ids.is_empty() {
        0
    } else {
        app.selected.min(ids.len() - 1)
    };
    let selected = app.selected;

    // Columns match the GUI model (`MyTableModel::headerData`): a row-number
    // column standing in for the vertical header, then Type/Address/Name/
    // Test Result/Traffic.
    let header = Row::new(vec!["#", "Type", "Address", "Name", "Test Result", "Traffic"])
        .style(Style::default().bold())
        .bottom_margin(1);

    let rows: Vec<Row> = ids
        .iter()
        .enumerate()
        .filter_map(|(i, id)| {
            let p = app.profiles.get(id)?;
            let is_active = Some(*id) == app.active_profile_id && app.core_status == CoreStatus::Running;
            // The GUI marks the started row with "*" in the vertical header
            // and can show the profile id instead of the row number.
            let number = if is_active {
                if app.window.show_profile_id {
                    format!("*{id}")
                } else {
                    "*".to_string()
                }
            } else if app.window.show_profile_id {
                id.to_string()
            } else {
                (i + 1).to_string()
            };
            let mark = if app.checked.contains(id) { "✓" } else { " " };

            let (latency, latency_color) = match app.latencies.get(id) {
                Some(&l) if l < 0 => ("Unavailable".to_string(), Color::Red),
                Some(&l) if l > 0 => (
                    format!("{l} ms"),
                    if l <= 200 { Color::Green } else { Color::Yellow },
                ),
                _ => (String::new(), Color::DarkGray),
            };
            // A finished speed test replaces the latency column with speeds,
            // like the GUI's "Test Result" column does.
            let result = match (&p.dl_speed, &p.ul_speed) {
                (Some(dl), Some(ul)) if !dl.is_empty() || !ul.is_empty() => {
                    format!("▼{dl} ▲{ul}")
                }
                _ => latency,
            };

            let traffic = if p.traffic_ul > 0 || p.traffic_dl > 0 {
                format!(
                    "{}↑ {}↓",
                    ncore::model::format_bytes(p.traffic_ul as u64, false),
                    ncore::model::format_bytes(p.traffic_dl as u64, false)
                )
            } else {
                String::new()
            };

            let style = if is_active {
                Style::default().fg(Color::Green).bold()
            } else if app.checked.contains(id) {
                Style::default().fg(ACCENT)
            } else {
                Style::default()
            };
            Some(
                Row::new(vec![
                    Cell::from(format!("{mark}{number}")),
                    Cell::from(p.display_core_type()),
                    Cell::from(p.display_address()),
                    Cell::from(p.display_name_str()),
                    Cell::from(result).style(Style::default().fg(latency_color)),
                    Cell::from(traffic),
                ])
                .style(style),
            )
        })
        .collect();

    let rows_len = rows.len();
    let widths = [
        Constraint::Length(6),
        Constraint::Length(12),
        Constraint::Percentage(26),
        Constraint::Percentage(32),
        Constraint::Length(16),
        Constraint::Percentage(20),
    ];

    let checked = app.checked.len();
    let title = format!(
        " Profiles — {} {}{} ",
        app.current_group_name(),
        if rows_len == 1 {
            "1 item".to_string()
        } else {
            format!("{rows_len} items")
        },
        if checked > 0 {
            format!(", {checked} selected")
        } else {
            String::new()
        },
    );
    let border_style = if app.focus == Focus::Table {
        Style::default().fg(ACCENT)
    } else {
        Style::default().fg(Color::DarkGray)
    };
    let table = Table::new(rows, widths)
        .header(header)
        .block(
            Block::bordered()
                .border_type(BorderType::Rounded)
                .border_style(border_style)
                .title(title),
        )
        .row_highlight_style(Style::default().add_modifier(Modifier::REVERSED))
        .highlight_symbol("▌");

    app.table_state
        .select(if rows_len == 0 { None } else { Some(selected) });
    // Rows region for mouse hit-testing: border (1) + header (1) + margin (1).
    app.table_rows_area = Rect {
        x: area.x + 1,
        y: area.y + 3,
        width: area.width.saturating_sub(2),
        height: (rows_len as u16).min(area.height.saturating_sub(4)),
    };
    frame.render_stateful_widget(table, area, &mut app.table_state);
}

fn render_bottom_panel(frame: &mut Frame<'_>, area: Rect, app: &mut App) {
    let mut title_spans = Vec::new();
    let mut x = area.x + 2;
    app.bottom_tab_xranges.clear();
    for tab in BottomTab::ALL {
        let label = format!(" {} ", tab.label());
        let width = label.chars().count() as u16;
        let style = if tab == app.bottom_tab {
            Style::default().fg(Color::Black).bg(ACCENT).bold()
        } else {
            Style::default().fg(Color::DarkGray)
        };
        title_spans.push(Span::styled(label, style));
        app.bottom_tab_xranges.push((x, x + width, area.y));
        x += width;
    }
    if app.bottom_tab == BottomTab::Logs {
        title_spans.push(Span::styled(
            format!(
                " errors-only:{} auto-scroll:{} ",
                if app.window.errors_only { "on" } else { "off" },
                if app.window.auto_scroll_log { "on" } else { "off" },
            ),
            Style::default().fg(Color::DarkGray),
        ));
    }

    let border_style = if app.focus == Focus::Bottom {
        Style::default().fg(ACCENT)
    } else {
        Style::default().fg(Color::DarkGray)
    };
    let block = Block::bordered()
        .border_type(BorderType::Rounded)
        .border_style(border_style)
        .title(Line::from(title_spans));
    let inner = block.inner(area);
    frame.render_widget(block, area);

    match app.bottom_tab {
        BottomTab::Logs => render_log_lines(frame, inner, app),
        BottomTab::Connections => render_connections_table(frame, inner, app),
        BottomTab::Graph => render_traffic_graph(frame, inner, app),
    }
}

fn render_log_lines(frame: &mut Frame<'_>, area: Rect, app: &mut App) {
    let height = area.height as usize;
    let len = app.visible_logs().len();
    if app.log_cursor >= len {
        app.log_cursor = len.saturating_sub(1);
    }
    let logs = app.visible_logs();
    // Auto-scroll pins the view to the tail unless the user is browsing.
    let focused = app.focus == Focus::Bottom;
    let skip = if focused && !app.window.auto_scroll_log {
        app.log_cursor.saturating_sub(height / 2)
    } else if focused {
        // Keep the cursor visible even while auto-scrolling.
        len.saturating_sub(height).min(app.log_cursor)
    } else {
        len.saturating_sub(height)
    };

    let lines: Vec<Line> = logs
        .iter()
        .enumerate()
        .skip(skip)
        .take(height)
        .map(|(i, e)| {
            let mut style = if e.is_error {
                Style::default().fg(Color::Red)
            } else {
                Style::default()
            };
            if focused && i == app.log_cursor {
                style = style.add_modifier(Modifier::REVERSED);
            }
            Line::styled(e.display(), style)
        })
        .collect();

    if lines.is_empty() && app.window.errors_only {
        frame.render_widget(
            Paragraph::new("(no error lines — press e to show all)")
                .style(Style::default().fg(Color::DarkGray)),
            area,
        );
        return;
    }
    frame.render_widget(Paragraph::new(lines).wrap(Wrap { trim: false }), area);
}

fn render_connections_table(frame: &mut Frame<'_>, area: Rect, app: &App) {
    let header = Row::new(vec![
        "Outbound",
        "Net",
        "Proto",
        "Destination",
        "Process",
        "Upload",
        "Download",
    ])
    .style(Style::default().bold());
    let rows = app.connections.iter().map(|c| {
        Row::new(vec![
            Cell::from(c.outbound.clone()),
            Cell::from(c.network.clone()),
            Cell::from(c.protocol.clone()),
            Cell::from(c.dest.clone()),
            Cell::from(
                c.process
                    .rsplit(['/', '\\'])
                    .next()
                    .unwrap_or("")
                    .to_string(),
            ),
            Cell::from(ncore::model::format_bytes(c.upload as u64, false)),
            Cell::from(ncore::model::format_bytes(c.download as u64, false)),
        ])
    });
    let widths = [
        Constraint::Length(10),
        Constraint::Length(5),
        Constraint::Length(6),
        Constraint::Percentage(38),
        Constraint::Percentage(25),
        Constraint::Length(12),
        Constraint::Length(12),
    ];
    frame.render_widget(Table::new(rows, widths).header(header), area);
}

fn render_traffic_graph(frame: &mut Frame<'_>, area: Rect, app: &App) {
    use ratatui::widgets::Sparkline;

    let up: Vec<u64> = app.speed_history.iter().map(|(u, _)| *u).collect();
    let down: Vec<u64> = app.speed_history.iter().map(|(_, d)| *d).collect();
    let max = up.iter().chain(down.iter()).copied().max().unwrap_or(0);

    let [up_area, down_area] =
        Layout::vertical([Constraint::Percentage(50), Constraint::Percentage(50)]).areas(area);

    let label = |name: &str, bps: u64| {
        format!(
            " {name}: {} (peak {})",
            ncore::model::format_bytes(bps, true),
            ncore::model::format_bytes(max, true)
        )
    };
    let last_up = up.last().copied().unwrap_or(0);
    let last_down = down.last().copied().unwrap_or(0);

    let [up_label, up_graph] =
        Layout::vertical([Constraint::Length(1), Constraint::Min(1)]).areas(up_area);
    let [down_label, down_graph] =
        Layout::vertical([Constraint::Length(1), Constraint::Min(1)]).areas(down_area);

    frame.render_widget(Paragraph::new(label("▲ up", last_up)), up_label);
    frame.render_widget(
        Sparkline::default()
            .data(&up)
            .max(max)
            .style(Style::default().fg(Color::Green)),
        up_graph,
    );
    frame.render_widget(Paragraph::new(label("▼ down", last_down)), down_label);
    frame.render_widget(
        Sparkline::default()
            .data(&down)
            .max(max)
            .style(Style::default().fg(ACCENT)),
        down_graph,
    );
}

/// The three GUI status labels: `label_speed`, `label_running`, `label_inbound`.
fn render_status_bar(frame: &mut Frame<'_>, area: Rect, app: &App) {
    let status_color = match &app.core_status {
        CoreStatus::Running => Color::Green,
        CoreStatus::Stopped => Color::Yellow,
        CoreStatus::Error(_) => Color::Red,
        _ => Color::DarkGray,
    };

    let fmt_speeds = |t: &TrafficData| {
        let (up, down) = t.speeds();
        format!("▲{up} ▼{down}")
    };
    let fmt_total = |t: &TrafficData| {
        format!(
            "↑{} ↓{}",
            ncore::model::format_bytes(t.up as u64, false),
            ncore::model::format_bytes(t.down as u64, false)
        )
    };

    let testing = if app.url_test_running {
        format!(
            "  Testing {}/{}",
            app.url_test_done.len(),
            app.url_test_expected
        )
    } else if app.speed_test_running {
        "  Speed testing".to_string()
    } else {
        String::new()
    };

    // label_speed
    let line1 = Line::from(vec![
        Span::styled(" ● ", Style::default().fg(status_color)),
        Span::raw(format!("{} ", app.core_status)),
        Span::raw(format!(
            "| Proxy: {}  {}",
            fmt_speeds(&app.traffic_proxy),
            fmt_total(&app.traffic_proxy)
        )),
        Span::raw(format!(" | Direct: {}", fmt_speeds(&app.traffic_direct))),
        Span::styled(testing, Style::default().fg(Color::Yellow)),
    ]);

    // label_running + label_inbound
    let inbound = if app.spmode.is_tun() {
        format!("Inbound: Tun ({})", app.datastore.tun_address)
    } else {
        format!(
            "Inbound: mixed {}:{}",
            app.datastore.inbound_address, app.datastore.inbound_socks_port
        )
    };
    let route = app
        .active_chain()
        .map(|c| c.chain_name.clone())
        .unwrap_or_else(|| "-".into());
    let running = if app.active_profile_id.is_some() {
        format!("[{}] {}", app.current_group_name(), app.active_profile_name())
    } else {
        "Not Running".to_string()
    };
    let line2 = Line::from(vec![
        Span::raw(format!(" {running}")),
        Span::styled(
            format!("  |  Route: {route}  |  {inbound}"),
            Style::default().fg(Color::Gray),
        ),
    ]);

    let hint = |key: &'static str, desc: &'static str| -> Vec<Span<'static>> {
        vec![
            Span::styled(key, Style::default().fg(ACCENT)),
            Span::styled(format!(":{desc}  "), Style::default().fg(Color::DarkGray)),
        ]
    };
    let line3 = Line::from(
        [
            hint("F2-F7", "menu"),
            hint("Enter", "start"),
            hint("s", "stop"),
            hint("Space", "select"),
            hint("t", "test"),
            hint("T", "speed"),
            hint("u", "tun"),
            hint("x", "sysproxy"),
            hint("p", "import"),
            hint("y", "copy"),
            hint("d", "del"),
            hint("/", "search"),
            hint("S-Tab", "focus"),
            hint("?", "help"),
        ]
        .concat(),
    );
    frame.render_widget(Paragraph::new(vec![line1, line2, line3]), area);
}

fn render_menu_overlay(frame: &mut Frame<'_>, app: &App) {
    let Some(state) = &app.menu else { return };
    let anchor_x = app
        .menu_bar_xranges
        .get(state.root)
        .map(|(x0, _)| *x0)
        .unwrap_or(0);

    let mut x = anchor_x;
    let mut y = 1u16;
    for (depth, level) in state.levels.iter().enumerate() {
        let focused = depth == state.levels.len() - 1;
        let width = level.width().min(frame.area().width.saturating_sub(x));
        let height = (level.items.len() as u16 + 2).min(frame.area().height.saturating_sub(y));
        let area = Rect {
            x,
            y,
            width,
            height,
        };
        let block = Block::bordered()
            .border_type(BorderType::Rounded)
            .border_style(Style::default().fg(if focused { ACCENT } else { Color::DarkGray }))
            .style(Style::default().bg(Color::Indexed(235)));
        let inner = block.inner(area);
        frame.render_widget(Clear, area);
        frame.render_widget(block, area);

        let lines: Vec<Line> = level
            .items
            .iter()
            .enumerate()
            .map(|(i, item)| {
                if item.separator {
                    return Line::styled(
                        "─".repeat(inner.width as usize),
                        Style::default().fg(Color::DarkGray),
                    );
                }
                let check = match item.checked {
                    Some(true) => "[x] ",
                    Some(false) => "[ ] ",
                    None => "",
                };
                let arrow = if item.submenu.is_empty() { "" } else { " ▸" };
                let mut style = if !item.enabled {
                    Style::default().fg(Color::DarkGray)
                } else {
                    Style::default()
                };
                if focused && i == level.cursor {
                    style = style.bg(ACCENT).fg(Color::Black).bold();
                }
                Line::styled(format!("{check}{}{arrow}", item.label), style)
            })
            .collect();
        frame.render_widget(Paragraph::new(lines), inner);

        // Cascade the next submenu to the right.
        x = (x + width).min(frame.area().width.saturating_sub(10));
        y += level.cursor as u16;
    }
}

fn render_dialog(frame: &mut Frame<'_>, app: &App) {
    let Some(dialog) = &app.dialog else { return };
    match dialog {
        Dialog::Help => render_help(frame),
        Dialog::Settings => render_settings_dialog(frame, app),
        Dialog::Routes => render_routes_dialog(frame, app),
        Dialog::Groups => render_groups_dialog(frame, app),
        Dialog::Confirm(c) => render_confirm(frame, c),
        Dialog::Prompt(p) => render_prompt(frame, p),
        Dialog::Picker(p) => render_picker(frame, p),
        Dialog::Text {
            title,
            body,
            scroll,
        } => render_text_dialog(frame, title, body, *scroll),
    }
}

/// A bordered, cleared, centered modal. Returns the inner area.
fn modal(frame: &mut Frame<'_>, pct_x: u16, pct_y: u16, title: &str) -> Rect {
    let area = centered_rect(pct_x, pct_y, frame.area());
    let block = Block::bordered()
        .border_type(BorderType::Rounded)
        .border_style(Style::default().fg(ACCENT))
        .title(format!(" {title} "))
        .style(Style::default().bg(Color::Indexed(235)));
    let inner = block.inner(area);
    frame.render_widget(Clear, area);
    frame.render_widget(block, area);
    inner
}

fn render_settings_dialog(frame: &mut Frame<'_>, app: &App) {
    let inner = modal(
        frame,
        70,
        80,
        "Basic Settings — Enter: edit/toggle · Esc: close",
    );
    let rows = SETTINGS.iter().enumerate().map(|(i, def)| {
        let mut value = (def.get)(&app.datastore);
        if let Some(buf) = &app.settings_edit {
            if i == app.settings_sel {
                value = format!("{buf}▌");
            }
        }
        let style = if i == app.settings_sel {
            Style::default().add_modifier(Modifier::REVERSED)
        } else {
            Style::default()
        };
        let kind = match def.kind {
            SettingKind::Bool => "bool",
            SettingKind::Int => "int",
            SettingKind::Str => "str",
        };
        Row::new(vec![
            Cell::from(def.label),
            Cell::from(value),
            Cell::from(kind),
        ])
        .style(style)
    });
    // Keep the highlighted row on screen.
    let mut state = TableState::default();
    state.select(Some(app.settings_sel));
    frame.render_stateful_widget(
        Table::new(
            rows,
            [
                Constraint::Percentage(38),
                Constraint::Percentage(52),
                Constraint::Length(6),
            ],
        ),
        inner,
        &mut state,
    );
}

fn render_routes_dialog(frame: &mut Frame<'_>, app: &App) {
    let inner = modal(
        frame,
        80,
        70,
        "Routing Settings — Enter: activate · Esc: close",
    );
    let [list_area, detail_area] =
        Layout::horizontal([Constraint::Percentage(38), Constraint::Percentage(62)]).areas(inner);

    let rows = app.chains.iter().enumerate().map(|(i, c)| {
        let marker = if c.base.id == app.datastore.current_route_id {
            "●"
        } else {
            " "
        };
        let style = if i == app.routes_sel {
            Style::default().add_modifier(Modifier::REVERSED)
        } else {
            Style::default()
        };
        Row::new(vec![
            Cell::from(marker),
            Cell::from(c.chain_name.clone()),
            Cell::from(format!("{} rules", c.rules.len())),
        ])
        .style(style)
    });
    frame.render_widget(
        Table::new(
            rows,
            [
                Constraint::Length(2),
                Constraint::Percentage(64),
                Constraint::Percentage(32),
            ],
        ),
        list_area,
    );

    let mut lines: Vec<Line> = Vec::new();
    if let Some(chain) = app.chains.get(app.routes_sel) {
        for (i, r) in chain.rules.iter().enumerate() {
            let criteria = if !r.domain.is_empty() || !r.domain_suffix.is_empty() {
                [r.domain.join(", "), r.domain_suffix.join(", ")]
                    .iter()
                    .filter(|s| !s.is_empty())
                    .cloned()
                    .collect::<Vec<_>>()
                    .join(", ")
            } else if !r.ip_cidr.is_empty() {
                r.ip_cidr.join(", ")
            } else if !r.rule_set.is_empty() {
                r.rule_set.join(", ")
            } else {
                String::new()
            };
            let outbound = match r.outbound_id {
                ncore::model::OUTBOUND_PROXY => "proxy".to_string(),
                ncore::model::OUTBOUND_DIRECT => "direct".to_string(),
                ncore::model::OUTBOUND_BLOCK => "block".to_string(),
                id => format!("#{id}"),
            };
            lines.push(Line::from(format!(
                "{:>3}. {} [{} → {}]",
                i + 1,
                r.name,
                if criteria.is_empty() { "*" } else { &criteria },
                outbound,
            )));
        }
        lines.push(Line::from(""));
        lines.push(Line::styled(
            format!(
                "default outbound: {}",
                match chain.default_outbound_id {
                    ncore::model::OUTBOUND_DIRECT => "direct",
                    ncore::model::OUTBOUND_BLOCK => "block",
                    _ => "proxy",
                }
            ),
            Style::default().fg(Color::DarkGray),
        ));
        if chain.rules.is_empty() {
            lines.insert(0, Line::from("(no rules)"));
        }
    }
    frame.render_widget(
        Paragraph::new(lines)
            .wrap(Wrap { trim: false })
            .block(Block::bordered().border_type(BorderType::Rounded).title(" Rules ")),
        detail_area,
    );
}

fn render_groups_dialog(frame: &mut Frame<'_>, app: &App) {
    let inner = modal(
        frame,
        70,
        60,
        "Groups — Enter: switch · n: new · r: rename · u: URL · d: delete",
    );
    let rows = app.groups.iter().enumerate().map(|(i, g)| {
        let url = g
            .extra
            .as_ref()
            .and_then(|e| e.url.clone())
            .unwrap_or_default();
        let style = if i == app.groups_sel {
            Style::default().add_modifier(Modifier::REVERSED)
        } else {
            Style::default()
        };
        Row::new(vec![
            Cell::from(if i == app.current_group { "●" } else { " " }),
            Cell::from(g.name.clone()),
            Cell::from(g.profiles.len().to_string()),
            Cell::from(url),
        ])
        .style(style)
    });
    frame.render_widget(
        Table::new(
            rows,
            [
                Constraint::Length(2),
                Constraint::Percentage(30),
                Constraint::Length(7),
                Constraint::Percentage(58),
            ],
        )
        .header(Row::new(vec!["", "Group", "Items", "Subscription URL"]).style(Style::default().bold())),
        inner,
    );
}

fn render_confirm(frame: &mut Frame<'_>, confirm: &Confirm) {
    let inner = modal(frame, 55, 45, "Confirmation");
    let mut lines: Vec<Line> = confirm.message.lines().map(Line::from).collect();
    lines.push(Line::from(""));
    lines.push(Line::styled(
        "y / Enter: confirm     any other key: cancel",
        Style::default().fg(Color::DarkGray),
    ));
    frame.render_widget(Paragraph::new(lines).wrap(Wrap { trim: false }), inner);
}

fn render_prompt(frame: &mut Frame<'_>, prompt: &Prompt) {
    let inner = modal(frame, 60, 20, &prompt.title);
    frame.render_widget(
        Paragraph::new(vec![
            Line::from(""),
            Line::from(format!("  {}▌", prompt.buffer)),
            Line::from(""),
            Line::styled(
                "  Enter: confirm     Esc: cancel",
                Style::default().fg(Color::DarkGray),
            ),
        ]),
        inner,
    );
}

fn render_picker(frame: &mut Frame<'_>, picker: &Picker) {
    let inner = modal(frame, 55, 55, &picker.title);
    let lines: Vec<Line> = picker
        .items
        .iter()
        .enumerate()
        .map(|(i, (label, _))| {
            let style = if i == picker.cursor {
                Style::default().bg(ACCENT).fg(Color::Black).bold()
            } else {
                Style::default()
            };
            Line::styled(format!("  {label}"), style)
        })
        .collect();
    frame.render_widget(Paragraph::new(lines), inner);
}

fn render_text_dialog(frame: &mut Frame<'_>, title: &str, body: &str, scroll: u16) {
    let inner = modal(frame, 80, 80, title);
    frame.render_widget(
        Paragraph::new(body)
            .scroll((scroll, 0))
            .wrap(Wrap { trim: false }),
        inner,
    );
}

fn render_help(frame: &mut Frame<'_>) {
    let inner = modal(frame, 66, 85, "Help — Esc to close");
    let help_text = Text::from(
        r#"
  Menus (mirror the GUI's tool buttons)
    F2  Program        F3  Profiles     F4  Preferences
    F5  Routing        F6  Test         F7  Information
    F10                Open the menu bar (Esc closes it)
    ←/→ h/l            Previous/next menu · enter/leave submenu

  Profile table
    j/k ↑/↓ PgUp/PgDn  Navigate (mouse: click, wheel, double-click starts)
    g/G                Top/bottom
    Space              Toggle selection (actions apply to the selection)
    a / A              Select all / unselect all
    Enter              Start the profile under the cursor
    s                  Stop the core
    t / T              URL test / full speed test
    u / x              Toggle Tun / System Proxy
    p / y              Import from clipboard / copy share links
    c / m / d          Clone / move to group / delete
    U                  Update the current group's subscription
    r                  Reload configs from disk
    [ / ]              Previous/next group (mouse: click tabs)
    /                  Search

  Bottom panel
    Tab                Switch tab (Logs / Connections / Traffic Graph)
    Shift-Tab          Move focus between the table and the panel
    In Logs (focused):
      j/k g/G          Navigate lines
      e                Errors-only filter
      a                Auto-scroll
      R / Enter        Add a domain from this line to the routing profile
      y / c            Copy line / clear log

  Dialogs
    Preferences ▸ Basic Settings    Edit nekobox.cfg / routing settings
    Preferences ▸ Routing Settings  Pick the active routing profile
    Preferences ▸ Groups            Manage groups and subscription URLs

  q / Ctrl-C           Quit
"#,
    );
    frame.render_widget(Paragraph::new(help_text), inner);
}

fn centered_rect(percent_x: u16, percent_y: u16, rect: Rect) -> Rect {
    // `areas::<N>` panics unless N matches the constraint count, so both
    // splits are destructured in full and the middle rect taken.
    let [_, area, _] = Layout::vertical([
        Constraint::Percentage((100 - percent_y) / 2),
        Constraint::Percentage(percent_y),
        Constraint::Percentage((100 - percent_y) / 2),
    ])
    .areas(rect);
    let [_, area, _] = Layout::horizontal([
        Constraint::Percentage((100 - percent_x) / 2),
        Constraint::Percentage(percent_x),
        Constraint::Percentage((100 - percent_x) / 2),
    ])
    .areas(area);
    area
}

// ============================================================================
// Main loop
// ============================================================================

pub fn run(
    terminal: &mut Terminal<CrosstermBackend<std::io::Stdout>>,
    args: &Args,
) -> anyhow::Result<()> {
    let mut app = App::new(args);

    while app.running {
        app.drain_worker_events();
        terminal.draw(|f| render(f, &mut app))?;

        if event::poll(Duration::from_millis(100))? {
            match event::read()? {
                event::Event::Key(key) if key.kind == KeyEventKind::Press => app.handle_key(&key),
                event::Event::Mouse(me) => app.handle_mouse(&me),
                _ => {}
            }
        }

        app.maybe_tick();
    }

    let _ = app.worker.tx.send(Command::Shutdown);
    std::thread::sleep(Duration::from_millis(100));

    Ok(())
}
