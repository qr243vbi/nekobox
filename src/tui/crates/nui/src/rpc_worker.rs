//! RPC worker thread — owns the synchronous `nrpc::Core` connection.
//!
//! The Thrift client is blocking, so it lives on a dedicated thread.
//! The UI sends [`Command`]s and receives [`Event`]s over crossbeam
//! channels; the worker never touches UI state directly.

use crossbeam::channel::{Receiver, Sender};
use nrpc::{Core, CoreConfig, LoadConfigReq, TestReq};

/// Commands from the UI to the worker.
#[derive(Debug)]
pub enum Command {
    /// Connect to an already-running core.
    Connect { address: String, port: u16 },
    /// Connect to an already-running core over a Unix socket (unix only).
    ConnectUds { path: String },
    /// Launch the core process and connect to it.
    Launch { config: Box<CoreConfig> },
    /// (Re)start the core with the given sing-box config JSON.
    Start { config_json: String },
    /// Stop the core.
    Stop,
    /// Poll traffic stats.
    QueryStats,
    /// Poll active connections.
    ListConnections,
    /// Start a speed test on the given outbound (async on the core side —
    /// poll with `QuerySpeedTest`, exactly like the URL test flow).
    SpeedTest {
        config_json: String,
        tag: String,
        download_addr: String,
        timeout_ms: i32,
        mode: SpeedTestMode,
        /// Test the currently running outbound instead of `config_json`.
        test_current: bool,
    },
    /// Poll the running speed test once; `tag` is echoed back in
    /// `SpeedTestDone` so the UI can match the result.
    QuerySpeedTest { tag: String },
    /// Abort a running URL/speed test.
    StopTest,
    /// Enable or disable the system DNS override.
    SetSystemDns { enable: bool },
    /// Fetch and parse a subscription URL (blocking HTTP in the worker).
    /// `gid` identifies the group so the result lands in the group the update
    /// was started for, even if the user has switched tabs since.
    UpdateSubscription {
        gid: i32,
        url: String,
        user_agent: Option<String>,
    },
    /// Enable/disable the system proxy (address/port of the local inbound).
    SetSystemProxy {
        enable: bool,
        address: String,
        port: i32,
    },
    /// Start a URL test for the given outbound tags.
    UrlTest {
        config_json: String,
        tags: Vec<String>,
        url: String,
        max_concurrency: i32,
        timeout_ms: i32,
    },
    /// Poll URL test results.
    QueryUrlTest,
    /// Shut down: stop the core, kill a launched child, exit the thread.
    Shutdown,
}

/// Which speed test the GUI's Test menu entries map to.
///
/// Mirrors the `SpeedTestRequest` flag combinations used by
/// `MainWindow::speedtest_current_group` for each menu action.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SpeedTestMode {
    /// "Full test" — download + upload.
    Full,
    /// "Download test"
    Download,
    /// "Upload test"
    Upload,
    /// "Country test" — resolve the exit country only.
    Country,
    /// "Simple download test" — fetch `simple_dl_url`.
    SimpleDownload,
}

impl SpeedTestMode {
    pub fn label(&self) -> &'static str {
        match self {
            Self::Full => "full test",
            Self::Download => "download test",
            Self::Upload => "upload test",
            Self::Country => "country test",
            Self::SimpleDownload => "simple download test",
        }
    }
}

/// Events from the worker to the UI.
#[derive(Debug)]
pub enum Event {
    /// Core connection established; carries IsPrivileged result.
    Connected { privileged: bool },
    /// Core was started with a config.
    Started,
    /// The core rejected a config on start.
    StartFailed(String),
    /// Core was stopped.
    Stopped,
    /// Traffic per outbound tag (cumulative bytes since core start).
    Stats {
        ups: Vec<(String, i64)>,
        downs: Vec<(String, i64)>,
    },
    /// Active connections snapshot.
    Connections(Vec<ConnectionInfo>),
    /// Speed test finished (or failed).
    SpeedTestDone {
        tag: String,
        dl_speed: String,
        ul_speed: String,
        latency: i32,
        country: String,
        error: String,
    },
    /// Subscription fetched and parsed (carries the target group id).
    SubProfiles {
        gid: i32,
        entities: Vec<ncore::model::ProxyEntity>,
    },
    /// URL test results: (outbound_tag, latency_ms, error).
    UrlTestResults(Vec<(String, i32, String)>),
    /// A log line for the Logs pane.
    Log(String),
    /// An operation failed.
    Error(String),
}

/// A single active connection (mirrors `ConnectionMetaData`).
#[derive(Debug, Clone)]
pub struct ConnectionInfo {
    pub upload: i64,
    pub download: i64,
    pub outbound: String,
    pub network: String,
    pub dest: String,
    pub protocol: String,
    pub process: String,
}

/// Handle to the running worker thread.
pub struct WorkerHandle {
    pub tx: Sender<Command>,
    pub rx: Receiver<Event>,
}

/// Spawn the RPC worker thread.
pub fn spawn() -> WorkerHandle {
    let (cmd_tx, cmd_rx) = crossbeam::channel::unbounded::<Command>();
    let (ev_tx, ev_rx) = crossbeam::channel::unbounded::<Event>();

    std::thread::spawn(move || {
        Worker {
            core: None,
            child: None,
            tx: ev_tx,
        }
        .run(cmd_rx);
    });

    WorkerHandle { tx: cmd_tx, rx: ev_rx }
}

struct Worker {
    core: Option<Core>,
    child: Option<std::process::Child>,
    tx: Sender<Event>,
}

impl Worker {
    fn run(&mut self, rx: Receiver<Command>) {
        while let Ok(cmd) = rx.recv() {
            let shutdown = matches!(cmd, Command::Shutdown);
            self.handle(cmd);
            if shutdown {
                break;
            }
        }
        tracing::debug!("rpc worker exited");
    }

    fn send(&self, ev: Event) {
        let _ = self.tx.send(ev);
    }

    fn log(&self, msg: impl Into<String>) {
        self.send(Event::Log(msg.into()));
    }

    fn error(&self, msg: impl Into<String>) {
        self.send(Event::Error(msg.into()));
    }

    /// Forward the core's stdout/stderr lines to the Logs pane.
    fn capture_child_logs(&self, child: &mut std::process::Child) {
        use std::io::{BufRead, BufReader, Read};
        let streams: Vec<Box<dyn Read + Send>> = [
            child.stdout.take().map(|s| Box::new(s) as _),
            child.stderr.take().map(|s| Box::new(s) as _),
        ]
        .into_iter()
        .flatten()
        .collect();
        for stream in streams {
            let tx = self.tx.clone();
            std::thread::spawn(move || {
                let reader = BufReader::new(stream);
                for line in reader.lines() {
                    match line {
                        Ok(l) if !l.is_empty() => {
                            if tx.send(Event::Log(l)).is_err() {
                                break;
                            }
                        }
                        Ok(_) => {}
                        Err(_) => break,
                    }
                }
            });
        }
    }

    fn handle(&mut self, cmd: Command) {
        match cmd {
            Command::Connect { address, port } => {
                match Core::connect_tcp(&address, port).and_then(|mut c| {
                    let priv_ = c.is_privileged()?;
                    Ok((c, priv_))
                }) {
                    Ok((core, privileged)) => {
                        self.log(format!("connected to core at {address}:{port}"));
                        self.core = Some(core);
                        self.send(Event::Connected { privileged });
                    }
                    Err(e) => self.error(format!("connect failed: {e:#}")),
                }
            }
            Command::ConnectUds { path } => {
                #[cfg(unix)]
                match Core::connect_uds(&path).and_then(|mut c| {
                    let priv_ = c.is_privileged()?;
                    Ok((c, priv_))
                }) {
                    Ok((core, privileged)) => {
                        self.log(format!("connected to core at {path}"));
                        self.core = Some(core);
                        self.send(Event::Connected { privileged });
                    }
                    Err(e) => self.error(format!("connect failed: {e:#}")),
                }
                #[cfg(not(unix))]
                self.error(format!("unix sockets are not supported on this platform ({path})"));
            }
            Command::Launch { config } => match config.launch() {
                Ok((mut child, core)) => {
                    self.log("core process launched".to_string());
                    self.capture_child_logs(&mut child);
                    self.child = Some(child);
                    self.core = Some(core);
                    self.send(Event::Connected { privileged: false });
                }
                Err(e) => self.error(format!("core launch failed: {e:#}")),
            },
            Command::Start { config_json } => {
                let Some(core) = self.core.as_mut() else {
                    self.error("start: core not connected".to_string());
                    return;
                };
                let req = LoadConfigReq {
                    core_config: Some(config_json),
                    ..Default::default()
                };
                // Restart semantics: stop first (ignore errors — may not be running).
                let _ = core.stop();
                match core.start(req) {
                    Ok(()) => {
                        self.log("core started".to_string());
                        self.send(Event::Started);
                    }
                    Err(e) => self.send(Event::StartFailed(format!("{e:#}"))),
                }
            }
            Command::Stop => {
                let Some(core) = self.core.as_mut() else {
                    self.error("stop: core not connected".to_string());
                    return;
                };
                match core.stop() {
                    Ok(()) => {
                        self.log("core stopped".to_string());
                        self.send(Event::Stopped);
                    }
                    Err(e) => self.error(format!("stop failed: {e:#}")),
                }
            }
            Command::QueryStats => {
                let Some(core) = self.core.as_mut() else {
                    return;
                };
                match core.query_stats() {
                    Ok(resp) => {
                        let ups = resp
                            .ups
                            .map(|m| m.into_iter().collect())
                            .unwrap_or_default();
                        let downs = resp
                            .downs
                            .map(|m| m.into_iter().collect())
                            .unwrap_or_default();
                        self.send(Event::Stats { ups, downs });
                    }
                    Err(e) => self.error(format!("query stats failed: {e:#}")),
                }
            }
            Command::ListConnections => {
                let Some(core) = self.core.as_mut() else {
                    return;
                };
                match core.list_connections() {
                    Ok(resp) => {
                        let conns = resp
                            .connections
                            .unwrap_or_default()
                            .into_iter()
                            .map(|c| ConnectionInfo {
                                upload: c.upload.unwrap_or(0),
                                download: c.download.unwrap_or(0),
                                outbound: c.outbound.unwrap_or_default(),
                                network: c.network.unwrap_or_default(),
                                dest: c.dest.unwrap_or_default(),
                                protocol: c.protocol.unwrap_or_default(),
                                process: c.process.unwrap_or_default(),
                            })
                            .collect();
                        self.send(Event::Connections(conns));
                    }
                    Err(e) => self.error(format!("list connections failed: {e:#}")),
                }
            }
            Command::SetSystemDns { enable } => {
                let Some(core) = self.core.as_mut() else {
                    self.error("system dns: core not connected".to_string());
                    return;
                };
                // The RPC takes `clear`: true tears the override down.
                match core.set_system_dns(!enable) {
                    Ok(()) => self.log(format!(
                        "system dns {}",
                        if enable { "set" } else { "cleared" }
                    )),
                    Err(e) => self.error(format!("system dns failed: {e:#}")),
                }
            }
            Command::StopTest => {
                let Some(core) = self.core.as_mut() else {
                    return;
                };
                match core.stop_test() {
                    Ok(()) => self.log("testing stopped".to_string()),
                    Err(e) => self.error(format!("stop test failed: {e:#}")),
                }
            }
            Command::SpeedTest {
                config_json,
                tag,
                download_addr,
                timeout_ms,
                mode,
                test_current,
            } => {
                let Some(core) = self.core.as_mut() else {
                    self.error("speed test: core not connected".to_string());
                    return;
                };
                let req = nrpc::SpeedTestRequest {
                    config: Some(config_json),
                    outbound_tags: Some(vec![tag.clone()]),
                    test_current: Some(test_current),
                    use_default_outbound: Some(test_current),
                    test_download: Some(matches!(
                        mode,
                        SpeedTestMode::Full | SpeedTestMode::Download | SpeedTestMode::SimpleDownload
                    )),
                    test_upload: Some(matches!(
                        mode,
                        SpeedTestMode::Full | SpeedTestMode::Upload
                    )),
                    simple_download: Some(mode == SpeedTestMode::SimpleDownload),
                    simple_download_addr: Some(download_addr),
                    timeout_ms: Some(timeout_ms),
                    only_country: Some(mode == SpeedTestMode::Country),
                    country_concurrency: Some(if mode == SpeedTestMode::Country { 1 } else { 0 }),
                };
                if let Err(e) = core.speed_test(req) {
                    self.send(Event::SpeedTestDone {
                        tag,
                        dl_speed: String::new(),
                        ul_speed: String::new(),
                        latency: 0,
                        country: String::new(),
                        error: format!("speed test failed: {e:#}"),
                    });
                    return;
                }
                self.log(format!("{} started", mode.label()));
            }
            Command::QuerySpeedTest { tag } => {
                let Some(core) = self.core.as_mut() else {
                    return;
                };
                match core.query_speed_test() {
                    Ok(resp) => {
                        if resp.is_running.unwrap_or(true) {
                            return;
                        }
                        let r = resp.result.unwrap_or_default();
                        self.send(Event::SpeedTestDone {
                            tag,
                            dl_speed: r.dl_speed.unwrap_or_default(),
                            ul_speed: r.ul_speed.unwrap_or_default(),
                            latency: r.latency.unwrap_or(0),
                            country: r.server_country.unwrap_or_default(),
                            error: r.error.unwrap_or_default(),
                        });
                    }
                    Err(e) => self.send(Event::SpeedTestDone {
                        tag,
                        dl_speed: String::new(),
                        ul_speed: String::new(),
                        latency: 0,
                        country: String::new(),
                        error: format!("speed test query failed: {e:#}"),
                    }),
                }
            }
            Command::UpdateSubscription { gid, url, user_agent } => {
                match ncore::sub::update_subscription(&url, user_agent.as_deref()) {
                    Ok(parsed) => {
                        let entities =
                            parsed.into_iter().map(|p| p.entity).collect::<Vec<_>>();
                        self.send(Event::SubProfiles { gid, entities });
                    }
                    Err(e) => self.error(format!("subscription update failed: {e:#}")),
                }
            }
            Command::SetSystemProxy {
                enable,
                address,
                port,
            } => {
                let Some(core) = self.core.as_mut() else {
                    self.error("system proxy: core not connected".to_string());
                    return;
                };
                let result = if enable {
                    core.enable_system_proxy(nrpc::SystemProxy::new(
                        address,
                        port,
                        true, // support_socks
                    ))
                } else {
                    core.disable_system_proxy()
                };
                match result {
                    Ok(()) => self.log(format!(
                        "system proxy {}",
                        if enable { "enabled" } else { "disabled" }
                    )),
                    Err(e) => self.error(format!("system proxy failed: {e:#}")),
                }
            }
            Command::UrlTest {
                config_json,
                tags,
                url,
                max_concurrency,
                timeout_ms,
            } => {
                let Some(core) = self.core.as_mut() else {
                    self.error("url test: core not connected".to_string());
                    return;
                };
                let req = TestReq {
                    config: Some(config_json),
                    outbound_tags: Some(tags),
                    use_default_outbound: Some(false),
                    url: Some(url),
                    test_current: Some(false),
                    max_concurrency: Some(max_concurrency),
                    test_timeout_ms: Some(timeout_ms),
                };
                match core.test(req) {
                    Ok(_) => self.log("url test started".to_string()),
                    Err(e) => self.error(format!("url test failed: {e:#}")),
                }
            }
            Command::QueryUrlTest => {
                let Some(core) = self.core.as_mut() else {
                    return;
                };
                match core.query_url_test() {
                    Ok(resp) => {
                        let results = resp
                            .results
                            .unwrap_or_default()
                            .into_iter()
                            .map(|r| {
                                (
                                    r.outbound_tag.unwrap_or_default(),
                                    r.latency_ms.unwrap_or(0),
                                    r.error.unwrap_or_default(),
                                )
                            })
                            .collect::<Vec<_>>();
                        if !results.is_empty() {
                            self.send(Event::UrlTestResults(results));
                        }
                    }
                    Err(e) => self.error(format!("query url test failed: {e:#}")),
                }
            }
            Command::Shutdown => {
                if let Some(core) = self.core.as_mut() {
                    let _ = core.stop();
                }
                if let Some(mut child) = self.child.take() {
                    let _ = child.kill();
                    let _ = child.wait();
                }
            }
        }
    }
}
