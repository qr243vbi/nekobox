//! Application state, event loop, and key routing.
//!
//! The app is structured as a state machine with switchable panes:
//! 1. Proxies (default) — proxy table, group selector
//! 2. Connections — active connections view
//! 3. Logs — core log output
//! 4. Routes — routing chain manager
//! 5. Settings — DataStore settings editor
//!
//! Only the Proxies and Logs panes are functional in the MVP; the rest
//! are placeholders.

use crate::cli::Args;
use crate::rpc_worker::{Command, Event, WorkerHandle};
use crossterm::event::{self, KeyCode, KeyEventKind, KeyModifiers};
use ncore::model::{DataStore, Group, ProxyEntity, TrafficData};
use ratatui::prelude::*;
use ratatui::widgets::{
    Block, BorderType, Cell, Clear, Paragraph, Row, Table, TableState, Wrap,
};
use std::collections::{HashMap, HashSet, VecDeque};
use std::path::PathBuf;
use std::time::{Duration, Instant};

const MAX_LOG_LINES: usize = 200;
const STATS_INTERVAL: Duration = Duration::from_secs(1);
const URL_TEST_TIMEOUT: Duration = Duration::from_secs(45);
const SPEED_HISTORY_CAP: usize = 240;

/// The active pane/view.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Pane {
    /// Proxy table (pane 1)
    Proxies,
    /// Active connections (pane 2)
    Connections,
    /// Core log output (pane 3)
    Logs,
    /// Routing chains (pane 4)
    Routes,
    /// Settings (pane 5)
    Settings,
}

impl Pane {
    /// Get the display label.
    pub fn label(&self) -> &'static str {
        match self {
            Self::Proxies => "Proxies",
            Self::Connections => "Connections",
            Self::Logs => "Logs",
            Self::Routes => "Routes",
            Self::Settings => "Settings",
        }
    }

    const ALL: [Pane; 5] = [
        Self::Proxies,
        Self::Connections,
        Self::Logs,
        Self::Routes,
        Self::Settings,
    ];
}

/// The bottom panel tab (inside the Proxies pane).
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

/// Core connection status.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CoreStatus {
    /// Connecting to core
    Connecting,
    /// Core is running
    Running,
    /// Core stopped
    Stopped,
    /// Error connecting
    Error(String),
}

impl std::fmt::Display for CoreStatus {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Connecting => write!(f, "Connecting..."),
            Self::Running => write!(f, "Running"),
            Self::Stopped => write!(f, "Stopped"),
            Self::Error(msg) => write!(f, "Error: {msg}"),
        }
    }
}

/// The application state.
pub struct App {
    /// Whether the app should quit
    pub running: bool,

    /// Currently active pane
    pub active_pane: Pane,

    /// Base config directory (contains profiles/, groups/, nekobox.cfg)
    config_dir: PathBuf,

    /// Global settings
    datastore: DataStore,

    /// All groups (sorted by id)
    groups: Vec<Group>,

    /// All profiles, keyed by id
    profiles: HashMap<i32, ProxyEntity>,

    /// Current group index into `groups`
    current_group: usize,

    /// Selected row in the (filtered) proxy table
    selected: usize,

    /// Table widget state (selection)
    table_state: TableState,

    /// Latency per profile id (ms; from URL tests)
    latencies: HashMap<i32, i32>,

    /// Core connection status
    core_status: CoreStatus,

    /// Currently activated profile id
    active_profile_id: Option<i32>,

    /// Traffic trackers per outbound tag (cumulative → speeds)
    traffic_proxy: TrafficData,
    traffic_direct: TrafficData,

    /// Recent proxy speeds (B/s) for the traffic graph
    speed_history: VecDeque<(u64, u64)>,

    /// Active connections snapshot
    connections: Vec<crate::rpc_worker::ConnectionInfo>,

    /// TUN mode enabled (used when building the next Start config)
    tun_mode: bool,

    /// System proxy enabled (best-effort local state)
    system_proxy: bool,

    /// Active bottom tab in the Proxies pane
    bottom_tab: BottomTab,

    /// Core log lines
    logs: VecDeque<String>,

    /// Help overlay
    show_help: bool,

    /// Filter input mode active
    filter_mode: bool,

    /// Filter/search string
    filter: String,

    /// URL test in flight
    url_test_running: bool,

    /// Profile tags already reported by the URL test
    url_test_done: HashSet<String>,

    /// Profile tags expected from the URL test
    url_test_expected: usize,

    /// When the URL test was started
    url_test_started: Option<Instant>,

    /// Last stats poll
    last_stats_poll: Instant,

    /// RPC worker
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

        let datastore = Self::load_datastore(&config_dir);
        let groups = Self::load_groups(&config_dir);
        let profiles = Self::load_profiles(&config_dir);
        let tun_mode = datastore.enable_tun_routing;

        let worker = crate::rpc_worker::spawn();

        // Connect or launch the core.
        let core_status = CoreStatus::Connecting;
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

        let mut app = Self {
            running: true,
            active_pane: Pane::Proxies,
            config_dir,
            datastore,
            groups,
            profiles,
            current_group: 0,
            selected: 0,
            table_state: TableState::default(),
            latencies: HashMap::new(),
            core_status,
            active_profile_id: None,
            traffic_proxy: TrafficData::new("proxy"),
            traffic_direct: TrafficData::new("direct"),
            speed_history: VecDeque::with_capacity(SPEED_HISTORY_CAP),
            connections: Vec::new(),
            tun_mode,
            system_proxy: false,
            bottom_tab: BottomTab::Logs,
            logs: VecDeque::new(),
            show_help: false,
            filter_mode: false,
            filter: String::new(),
            url_test_running: false,
            url_test_done: HashSet::new(),
            url_test_expected: 0,
            url_test_started: None,
            last_stats_poll: Instant::now(),
            worker,
        };
        app.log(format!("config dir: {}", app.config_dir.display()));
        app.log(format!(
            "loaded {} groups, {} profiles",
            app.groups.len(),
            app.profiles.len()
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

    fn load_datastore(base: &std::path::Path) -> DataStore {
        let path = base.join("nekobox.cfg");
        ncore::store::load_datastore(&path).unwrap_or_default()
    }

    fn load_groups(base: &std::path::Path) -> Vec<Group> {
        let mut groups = Vec::new();
        if let Ok(files) = ncore::store::list_store_files(base, "groups") {
            for (id, path) in files {
                if let Ok(mut g) = ncore::store::load_group(&path) {
                    // Attach subscription extras (subscriptions/<id>.cfg).
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
                    // Attach the protocol bean (beans/<id>.cfg).
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

    fn reload(&mut self) {
        self.datastore = Self::load_datastore(&self.config_dir);
        self.groups = Self::load_groups(&self.config_dir);
        self.profiles = Self::load_profiles(&self.config_dir);
        self.current_group = self.current_group.min(self.groups.len().saturating_sub(1));
        self.selected = 0;
        self.seed_latencies();
        self.log(format!(
            "reloaded: {} groups, {} profiles",
            self.groups.len(),
            self.profiles.len()
        ));
    }

    // ------------------------------------------------------------------
    // View helpers
    // ------------------------------------------------------------------

    /// Profiles of the current group, in group order, after filtering.
    fn visible_profiles(&self) -> Vec<&ProxyEntity> {
        let Some(group) = self.groups.get(self.current_group) else {
            return Vec::new();
        };
        let filter = self.filter.to_lowercase();
        group
            .profiles
            .iter()
            .filter_map(|id| self.profiles.get(id))
            .filter(|p| {
                filter.is_empty()
                    || p.name.to_lowercase().contains(&filter)
                    || p.server_address.to_lowercase().contains(&filter)
                    || p.display_core_type().to_lowercase().contains(&filter)
            })
            .collect()
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
            .unwrap_or_else(|| "none".into())
    }

    fn log(&mut self, msg: String) {
        if self.logs.len() >= MAX_LOG_LINES {
            self.logs.pop_front();
        }
        self.logs.push_back(msg);
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
                }
                Event::Started => {
                    self.core_status = CoreStatus::Running;
                }
                Event::Stopped => {
                    self.core_status = CoreStatus::Stopped;
                    self.active_profile_id = None;
                    self.traffic_proxy = TrafficData::new("proxy");
                    self.traffic_direct = TrafficData::new("direct");
                    self.connections.clear();
                }
                Event::Stats { ups, downs } => {
                    let sum = |map: &[(String, i64)], tag: &str| {
                        map.iter()
                            .filter(|(k, _)| k == tag)
                            .map(|(_, v)| *v)
                            .sum()
                    };
                    let prev_up = self.traffic_proxy.up;
                    let prev_down = self.traffic_proxy.down;
                    self.traffic_proxy
                        .update(sum(&ups, "proxy"), sum(&downs, "proxy"));
                    self.traffic_direct
                        .update(sum(&ups, "direct"), sum(&downs, "direct"));
                    // Graph history: bytes/sec delta of the proxy tag.
                    let dt = STATS_INTERVAL.as_secs_f64();
                    let up_bps = ((self.traffic_proxy.up - prev_up) as f64 / dt) as u64;
                    let down_bps = ((self.traffic_proxy.down - prev_down) as f64 / dt) as u64;
                    if self.speed_history.len() >= SPEED_HISTORY_CAP {
                        self.speed_history.pop_front();
                    }
                    self.speed_history.push_back((up_bps, down_bps));
                }
                Event::Connections(conns) => {
                    self.connections = conns;
                }
                Event::UrlTestResults(results) => {
                    for (tag, latency, error) in results {
                        if !self.url_test_done.insert(tag.clone()) {
                            continue;
                        }
                        if let Ok(id) = tag.parse::<i32>() {
                            self.latencies.insert(
                                id,
                                if error.is_empty() { latency } else { -1 },
                            );
                        }
                    }
                    if self.url_test_done.len() >= self.url_test_expected {
                        self.url_test_running = false;
                        self.log("url test finished".into());
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

    /// Periodic work: stats polling, URL test polling.
    pub fn maybe_tick(&mut self) {
        let now = Instant::now();
        if now.duration_since(self.last_stats_poll) < STATS_INTERVAL {
            return;
        }
        self.last_stats_poll = now;

        if self.core_status == CoreStatus::Running {
            let _ = self.worker.tx.send(Command::QueryStats);
            let _ = self.worker.tx.send(Command::ListConnections);
        }

        if self.url_test_running {
            // Give up on a hung test.
            if self
                .url_test_started
                .map(|t| t.elapsed() > URL_TEST_TIMEOUT)
                .unwrap_or(false)
            {
                self.url_test_running = false;
                self.log("url test timed out".into());
            } else {
                let _ = self.worker.tx.send(Command::QueryUrlTest);
            }
        }
    }

    // ------------------------------------------------------------------
    // Actions
    // ------------------------------------------------------------------

    fn activate_selected(&mut self) {
        self.datastore.enable_tun_routing = self.tun_mode;
        let visible = self.visible_profiles();
        let Some(profile) = visible.get(self.selected) else {
            return;
        };
        let id = profile.id;
        match ncore::config::build_config(profile, &self.datastore) {
            Ok(config) => {
                self.log(format!("starting: {}", profile.display_type_and_name()));
                self.active_profile_id = Some(id);
                self.core_status = CoreStatus::Connecting;
                let _ = self.worker.tx.send(Command::Start {
                    config_json: config.to_string(),
                });
            }
            Err(e) => self.log(format!("config build failed: {e:#}")),
        }
    }

    fn stop_core(&mut self) {
        let _ = self.worker.tx.send(Command::Stop);
    }

    fn toggle_tun(&mut self) {
        self.tun_mode = !self.tun_mode;
        self.log(format!(
            "tun mode {}",
            if self.tun_mode { "on" } else { "off" }
        ));
        // Restart with the new inbound if the core is running.
        if self.core_status != CoreStatus::Running {
            return;
        }
        if let Some(profile) = self
            .active_profile_id
            .and_then(|id| self.profiles.get(&id))
            .cloned()
        {
            self.datastore.enable_tun_routing = self.tun_mode;
            match ncore::config::build_config(&profile, &self.datastore) {
                Ok(config) => {
                    let _ = self.worker.tx.send(Command::Start {
                        config_json: config.to_string(),
                    });
                }
                Err(e) => self.log(format!("config build failed: {e:#}")),
            }
        }
    }

    fn toggle_system_proxy(&mut self) {
        self.system_proxy = !self.system_proxy;
        let _ = self.worker.tx.send(Command::SetSystemProxy {
            enable: self.system_proxy,
            address: self.datastore.inbound_address.clone(),
            port: self.datastore.inbound_socks_port,
        });
    }

    fn cycle_bottom_tab(&mut self) {
        let current = BottomTab::ALL
            .iter()
            .position(|t| *t == self.bottom_tab)
            .unwrap_or(0);
        self.bottom_tab = BottomTab::ALL[(current + 1) % BottomTab::ALL.len()];
    }

    fn url_test_group(&mut self) {
        if self.url_test_running {
            return;
        }
        let group_profiles: Vec<&ProxyEntity> = match self.groups.get(self.current_group) {
            Some(g) => g
                .profiles
                .iter()
                .filter_map(|id| self.profiles.get(id))
                .collect(),
            None => Vec::new(),
        };
        if group_profiles.is_empty() {
            self.log("url test: no profiles in group".into());
            return;
        }
        let (config_json, tags) = ncore::config::build_test_config(&group_profiles);
        self.url_test_running = true;
        self.url_test_expected = tags.len();
        self.url_test_done.clear();
        self.url_test_started = Some(Instant::now());
        self.log(format!("url test: {} profiles", tags.len()));
        let _ = self.worker.tx.send(Command::UrlTest {
            config_json,
            tags,
            url: self.datastore.test_latency_url.clone(),
            max_concurrency: self.datastore.test_concurrent,
            timeout_ms: self.datastore.url_test_timeout_ms,
        });
    }

    // ------------------------------------------------------------------
    // Input
    // ------------------------------------------------------------------

    /// Handle a key event.
    pub fn handle_key(&mut self, key: &event::KeyEvent) {
        // Filter input mode captures everything.
        if self.filter_mode {
            self.handle_filter_key(key);
            return;
        }

        // Help overlay captures everything.
        if self.show_help {
            match key.code {
                KeyCode::Esc | KeyCode::Char('q') | KeyCode::Char('?') => self.show_help = false,
                _ => {}
            }
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
            KeyCode::Char('?') => {
                self.show_help = true;
                return;
            }
            KeyCode::Tab => {
                if self.active_pane == Pane::Proxies {
                    self.cycle_bottom_tab();
                } else {
                    self.cycle_pane(true);
                }
                return;
            }
            KeyCode::BackTab => {
                self.cycle_pane(false);
                return;
            }
            KeyCode::Char(c @ '1'..='5') => {
                self.active_pane = Pane::ALL[(c as usize) - ('1' as usize)];
                return;
            }
            _ => {}
        }

        // Pane-specific keys
        if self.active_pane == Pane::Proxies {
            self.handle_proxies_key(key);
        }
    }

    fn handle_filter_key(&mut self, key: &event::KeyEvent) {
        match key.code {
            KeyCode::Esc => {
                self.filter_mode = false;
                self.filter.clear();
                self.selected = 0;
            }
            KeyCode::Enter => {
                self.filter_mode = false;
            }
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

    fn handle_proxies_key(&mut self, key: &event::KeyEvent) {
        let count = self.visible_profiles().len();
        match key.code {
            KeyCode::Char('j') | KeyCode::Down => {
                if count > 0 {
                    self.selected = (self.selected + 1).min(count - 1);
                }
            }
            KeyCode::Char('k') | KeyCode::Up => {
                self.selected = self.selected.saturating_sub(1);
            }
            KeyCode::Char('g') => self.selected = 0,
            KeyCode::Char('G') => {
                self.selected = count.saturating_sub(1);
            }
            KeyCode::Enter => self.activate_selected(),
            KeyCode::Char('s') => self.stop_core(),
            KeyCode::Char('t') => self.url_test_group(),
            KeyCode::Char('u') => self.toggle_tun(),
            KeyCode::Char('x') => self.toggle_system_proxy(),
            KeyCode::Char('r') => self.reload(),
            KeyCode::Char('[') => self.switch_group(false),
            KeyCode::Char(']') => self.switch_group(true),
            KeyCode::Char('/') => {
                self.filter_mode = true;
            }
            _ => {}
        }
    }

    fn switch_group(&mut self, forward: bool) {
        if self.groups.is_empty() {
            return;
        }
        let len = self.groups.len();
        self.current_group = if forward {
            (self.current_group + 1) % len
        } else {
            (self.current_group + len - 1) % len
        };
        self.selected = 0;
    }

    fn cycle_pane(&mut self, forward: bool) {
        let current = Pane::ALL
            .iter()
            .position(|p| *p == self.active_pane)
            .unwrap_or(0);
        let idx = if forward {
            (current + 1) % Pane::ALL.len()
        } else {
            (current + Pane::ALL.len() - 1) % Pane::ALL.len()
        };
        self.active_pane = Pane::ALL[idx];
    }

    fn quit(&mut self) {
        let _ = self.worker.tx.send(Command::Shutdown);
        self.running = false;
    }
}

// ============================================================================
// Rendering
// ============================================================================

/// Render the application UI.
pub fn render(frame: &mut Frame<'_>, app: &mut App) {
    let [header_area, content_area, status_area] = Layout::vertical([
        Constraint::Length(2),
        Constraint::Min(5),
        Constraint::Length(3),
    ])
    .areas(frame.area());

    render_header(frame, header_area, app);

    match app.active_pane {
        Pane::Proxies => render_proxies_pane(frame, content_area, app),
        Pane::Connections => {
            let block = Block::bordered()
                .border_type(BorderType::Rounded)
                .title(" Connections (2) ");
            let inner = block.inner(content_area);
            frame.render_widget(block, content_area);
            render_connections_table(frame, inner, app);
        }
        Pane::Logs => render_logs_pane(frame, content_area, app),
        Pane::Routes => render_placeholder(frame, content_area, "Routes (4)"),
        Pane::Settings => render_placeholder(frame, content_area, "Settings (5)"),
    }

    render_status_bar(frame, status_area, app);

    if app.filter_mode {
        render_filter(frame, app);
    }
    if app.show_help {
        render_help(frame);
    }
}

fn render_header(frame: &mut Frame<'_>, area: Rect, app: &App) {
    let mut spans = vec![Span::styled(
        format!(" NekoBox TUI [{}]  ", app.active_pane.label()),
        Style::default().add_modifier(Modifier::BOLD),
    )];
    for (i, g) in app.groups.iter().enumerate() {
        let style = if i == app.current_group {
            Style::default()
                .fg(Color::Black)
                .bg(Color::Cyan)
                .add_modifier(Modifier::BOLD)
        } else {
            Style::default().fg(Color::DarkGray)
        };
        spans.push(Span::styled(format!(" {} ", g.name), style));
        spans.push(Span::raw(" "));
    }
    frame.render_widget(Paragraph::new(Line::from(spans)), area);
}

fn render_proxies_pane(frame: &mut Frame<'_>, area: Rect, app: &mut App) {
    let [table_area, bottom_area] =
        Layout::vertical([Constraint::Min(6), Constraint::Length(11)]).areas(area);

    let count = app.visible_profiles().len();
    app.selected = if count == 0 { 0 } else { app.selected.min(count - 1) };
    let selected = app.selected;
    let visible = app.visible_profiles();

    let header = Row::new(vec!["", "Type", "Address", "Name", "Test Result", "Traffic"])
        .style(Style::default().add_modifier(Modifier::BOLD))
        .bottom_margin(1);

    let rows = visible.iter().map(|p| {
        let is_active = Some(p.id) == app.active_profile_id
            && app.core_status == CoreStatus::Running;
        let marker = if is_active { "▶" } else { "" };

        let (latency, latency_style) = match app.latencies.get(&p.id) {
            Some(&l) if l < 0 => ("Unavailable".to_string(), Color::Red),
            Some(&l) if l > 0 => {
                let color = if l <= 200 { Color::Green } else { Color::Yellow };
                (format!("{l} ms"), color)
            }
            _ => (String::new(), Color::DarkGray),
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
            Style::default().fg(Color::Green)
        } else {
            Style::default()
        };
        Row::new(vec![
            Cell::from(marker),
            Cell::from(p.display_core_type()),
            Cell::from(p.display_address()),
            Cell::from(p.display_name_str()),
            Cell::from(latency).style(Style::default().fg(latency_style)),
            Cell::from(traffic),
        ])
        .style(style)
    });

    let widths = [
        Constraint::Length(2),
        Constraint::Length(11),
        Constraint::Percentage(28),
        Constraint::Percentage(32),
        Constraint::Length(12),
        Constraint::Percentage(24),
    ];

    let group_name = app.current_group_name();
    let title = format!(" Proxies (1) — {group_name} ");
    let table = Table::new(rows, widths)
        .header(header)
        .block(Block::bordered().border_type(BorderType::Rounded).title(title))
        .row_highlight_style(Style::default().add_modifier(Modifier::REVERSED))
        .highlight_symbol("> ");

    app.table_state.select(if visible.is_empty() {
        None
    } else {
        Some(selected)
    });
    frame.render_stateful_widget(table, table_area, &mut app.table_state);

    render_bottom_panel(frame, bottom_area, app);
}

/// Bottom panel with Logs / Connections / Traffic Graph tabs.
fn render_bottom_panel(frame: &mut Frame<'_>, area: Rect, app: &App) {
    // Tab strip in the block title.
    let mut title_spans = Vec::new();
    for tab in BottomTab::ALL {
        let style = if tab == app.bottom_tab {
            Style::default()
                .fg(Color::Black)
                .bg(Color::Cyan)
                .add_modifier(Modifier::BOLD)
        } else {
            Style::default().fg(Color::DarkGray)
        };
        title_spans.push(Span::styled(format!(" {} ", tab.label()), style));
    }
    title_spans.push(Span::raw(" (Tab to switch)"));

    let block = Block::bordered()
        .border_type(BorderType::Rounded)
        .title(Line::from(title_spans));
    let inner = block.inner(area);
    frame.render_widget(block, area);

    match app.bottom_tab {
        BottomTab::Logs => render_log_lines(frame, inner, app),
        BottomTab::Connections => render_connections_table(frame, inner, app),
        BottomTab::Graph => render_traffic_graph(frame, inner, app),
    }
}

fn render_log_lines(frame: &mut Frame<'_>, area: Rect, app: &App) {
    let height = area.height as usize;
    let skip = app.logs.len().saturating_sub(height);
    let text: Vec<Line> = app
        .logs
        .iter()
        .skip(skip)
        .map(|l| Line::from(l.clone()))
        .collect();
    frame.render_widget(Paragraph::new(text).wrap(Wrap { trim: false }), area);
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
    .style(Style::default().add_modifier(Modifier::BOLD));
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
    let table = Table::new(rows, widths).header(header);
    frame.render_widget(table, area);
}

fn render_traffic_graph(frame: &mut Frame<'_>, area: Rect, app: &App) {
    use ratatui::widgets::Sparkline;

    let up: Vec<u64> = app.speed_history.iter().map(|(u, _)| *u).collect();
    let down: Vec<u64> = app.speed_history.iter().map(|(_, d)| *d).collect();
    let max = up
        .iter()
        .chain(down.iter())
        .copied()
        .max()
        .unwrap_or(0);

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
            .style(Style::default().fg(Color::Cyan)),
        down_graph,
    );
}

fn render_logs_pane(frame: &mut Frame<'_>, area: Rect, app: &App) {
    let block = Block::bordered()
        .border_type(BorderType::Rounded)
        .title(" Logs (3) ");
    let inner = block.inner(area);
    frame.render_widget(block, area);
    render_log_lines(frame, inner, app);
}

fn render_placeholder(frame: &mut Frame<'_>, area: Rect, title: &str) {
    let block = Block::bordered()
        .border_type(BorderType::Rounded)
        .title(format!(" {title} — coming soon "));
    frame.render_widget(Paragraph::new("").block(block), area);
}

fn render_status_bar(frame: &mut Frame<'_>, area: Rect, app: &App) {
    let status_color = match &app.core_status {
        CoreStatus::Running => Color::Green,
        CoreStatus::Stopped => Color::Yellow,
        CoreStatus::Error(_) => Color::Red,
        _ => Color::DarkGray,
    };
    let testing = if app.url_test_running {
        format!(
            " | testing {}/{}",
            app.url_test_done.len(),
            app.url_test_expected
        )
    } else {
        String::new()
    };
    let filter = if app.filter.is_empty() {
        String::new()
    } else {
        format!(" | filter: {}", app.filter)
    };

    let fmt_speeds = |t: &TrafficData| {
        let (up, down) = t.speeds();
        format!(
            "▲{} ▼{}",
            up.unwrap_or("0 B/s"),
            down.unwrap_or("0 B/s")
        )
    };
    let fmt_total = |t: &TrafficData| {
        format!(
            "↑{} ↓{}",
            ncore::model::format_bytes(t.up as u64, false),
            ncore::model::format_bytes(t.down as u64, false)
        )
    };

    let inbound = if app.tun_mode {
        "Inbound: TUN".to_string()
    } else {
        format!(
            "Inbound: mixed {}:{}",
            app.datastore.inbound_address, app.datastore.inbound_socks_port
        )
    };
    let flags = format!(
        "{}{}",
        if app.tun_mode { " [TUN]" } else { "" },
        if app.system_proxy { " [SysProxy]" } else { "" },
    );

    let line1 = Line::from(vec![
        Span::styled(" ● ", Style::default().fg(status_color)),
        Span::raw(format!("{} ", app.core_status)),
        Span::raw(format!(
            "| Proxy: {}  {}",
            fmt_speeds(&app.traffic_proxy),
            fmt_total(&app.traffic_proxy)
        )),
        Span::raw(format!(" | Direct: {}{}", fmt_speeds(&app.traffic_direct), testing)),
    ]);
    let line2 = Line::from(vec![
        Span::raw(format!(" [{}] {}", app.current_group_name(), app.active_profile_name())),
        Span::raw(format!(" | {inbound}{flags}{filter}")),
    ]);
    // htop-style key hints, always visible.
    let hint = |key: &'static str, desc: &'static str| -> Vec<Span<'static>> {
        vec![
            Span::styled(key, Style::default().fg(Color::Cyan)),
            Span::styled(format!(":{desc}  "), Style::default().fg(Color::DarkGray)),
        ]
    };
    let line3 = Line::from(
        [
            hint("Enter", "start"),
            hint("s", "stop"),
            hint("t", "test"),
            hint("u", "tun"),
            hint("x", "sysproxy"),
            hint("[/]", "group"),
            hint("/", "filter"),
            hint("Tab", "tabs"),
            hint("?", "help"),
            hint("q", "quit"),
        ]
        .concat(),
    );
    frame.render_widget(Paragraph::new(vec![line1, line2, line3]), area);
}

fn render_filter(frame: &mut Frame<'_>, app: &App) {
    let area = Rect {
        x: 0,
        y: frame.area().height.saturating_sub(4),
        width: frame.area().width,
        height: 1,
    };
    frame.render_widget(Clear, area);
    frame.render_widget(
        Paragraph::new(Line::from(vec![
            Span::styled("/", Style::default().fg(Color::Cyan)),
            Span::raw(app.filter.clone()),
        ])),
        area,
    );
    frame.set_cursor_position((area.x + 1 + app.filter.chars().count() as u16, area.y));
}

fn render_help(frame: &mut Frame<'_>) {
    let area = centered_rect(60, 80, frame.area());
    let block = Block::bordered()
        .border_type(BorderType::Rounded)
        .title(" Help (?) — Esc to close ")
        .style(Style::default().bg(Color::Black));
    frame.render_widget(Clear, area);
    frame.render_widget(block, area);

    let help_text = Text::from(
        r#"
  Global:
    q / Ctrl+C       Quit
    ?                Toggle help
    Tab              Switch bottom tab (in Proxies) / next pane
    S-Tab            Previous pane
    1-5              Switch to pane

  Proxies pane:
    j/k ↑/↓          Navigate
    g/G              Top/bottom
    Enter            Activate proxy (start core)
    s                Stop core
    t                URL test (group)
    u                Toggle TUN mode
    x                Toggle system proxy
    r                Reload configs from disk
    [/]              Previous/next group
    /                Filter
"#,
    );
    frame.render_widget(Paragraph::new(help_text), area);
}

fn centered_rect(percent_x: u16, percent_y: u16, rect: Rect) -> Rect {
    let [area] = Layout::vertical([
        Constraint::Percentage((100 - percent_y) / 2),
        Constraint::Percentage(percent_y),
        Constraint::Percentage((100 - percent_y) / 2),
    ])
    .areas(rect);
    let [area] = Layout::horizontal([
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

/// Run the application main loop.
pub fn run(
    terminal: &mut Terminal<CrosstermBackend<std::io::Stdout>>,
    args: &Args,
) -> anyhow::Result<()> {
    let mut app = App::new(args);

    while app.running {
        app.drain_worker_events();
        terminal.draw(|f| render(f, &mut app))?;

        if event::poll(Duration::from_millis(100))? {
            if let event::Event::Key(key) = event::read()? {
                if key.kind == KeyEventKind::Press {
                    app.handle_key(&key);
                }
            }
        }

        app.maybe_tick();
    }

    // Give the worker a moment to stop the core / kill the child.
    let _ = app.worker.tx.send(Command::Shutdown);
    std::thread::sleep(Duration::from_millis(100));

    Ok(())
}
