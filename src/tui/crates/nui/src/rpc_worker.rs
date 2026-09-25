//! RPC worker — owns the connections to `nekobox_core`.
//!
//! The Thrift client is blocking, so the UI never talks to the core itself:
//! it sends [`Command`]s and receives [`Event`]s over crossbeam channels, and
//! the worker never touches UI state directly.
//!
//! Commands that change the core's state (connect, start, stop, stats, …) run
//! one after another, in order, on a single connection. Tests cannot share
//! it: the core's `Test`/`SpeedTest` calls only return once the whole batch is
//! done, which can take a minute. Each test therefore runs on its own thread
//! with its own connections — one blocked in the call, one polling for live
//! results — and `StopTest` goes out on a fresh connection, so stopping, stats
//! and start/stop never queue behind a test. That is the GUI's model too: a
//! new socket per call (`CHANNEL` in RPC.cpp) and tests on background threads.

use crossbeam::channel::{Receiver, RecvTimeoutError, Sender};
use nrpc::{Core, CoreConfig, Endpoint, LoadConfigReq, TestReq};
use std::collections::HashSet;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

/// How often a running URL test is polled for results (`runURLTest`: 200 ms).
const URL_TEST_POLL: Duration = Duration::from_millis(200);
/// How often a running speed test is polled for progress (`runSpeedTest`: 100 ms).
const SPEED_TEST_POLL: Duration = Duration::from_millis(100);

/// Commands from the UI to the worker.
#[derive(Debug)]
pub enum Command {
    /// Connect to an already-running core at the worker's endpoint.
    Connect,
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
    /// Enable or disable the system DNS override.
    SetSystemDns { enable: bool },
    /// Fetch and parse a subscription URL on a background thread.
    /// `gid` identifies the group so the result lands in the group the update
    /// was started for, even if the user has switched tabs since.
    UpdateSubscription {
        gid: i32,
        url: String,
        options: ncore::sub::FetchOptions,
    },
    /// Run a URL test over `batches`, one after another.
    UrlTest {
        /// Echoed back in every event of this test.
        seq: u64,
        batches: Vec<TestBatch>,
        url: String,
        max_concurrency: i32,
        timeout_ms: i32,
    },
    /// Run a speed test over `batches`, one after another.
    SpeedTest {
        /// Echoed back in every event of this test.
        seq: u64,
        batches: Vec<TestBatch>,
        mode: SpeedTestMode,
        download_addr: String,
        timeout_ms: i32,
        /// How many country lookups may run at once (`test_concurrent`).
        country_concurrency: i32,
        /// Test the running instance instead of each batch's config.
        test_current: bool,
    },
    /// Abort every running URL/speed test.
    StopTest,
    /// Shut down: stop tests and the core, kill a launched child, exit.
    Shutdown,
}

/// One test config and the outbound tags to test in it. The GUI builds a
/// config per 25 profiles, so one bad outbound only fails its own batch.
#[derive(Debug, Clone)]
pub struct TestBatch {
    pub config_json: String,
    pub tags: Vec<String>,
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

    /// The GUI's `speed_test_mode` setting (`Configs::TestConfig`).
    pub fn from_setting(mode: i32) -> Self {
        match mode {
            1 => Self::Download,
            2 => Self::Upload,
            3 => Self::SimpleDownload,
            4 => Self::Country,
            _ => Self::Full,
        }
    }
}

/// One URL test result (`URLTestResp`).
#[derive(Debug, Clone)]
pub struct UrlTestResult {
    pub tag: String,
    pub latency_ms: i32,
    pub error: String,
}

/// One speed/country test result (`SpeedTestResult`).
#[derive(Debug, Clone, Default)]
pub struct SpeedTestResult {
    pub tag: String,
    pub dl_speed: String,
    pub ul_speed: String,
    pub latency: i32,
    pub server_name: String,
    pub server_country: String,
    pub error: String,
    pub cancelled: bool,
}

impl From<nrpc::SpeedTestResult> for SpeedTestResult {
    fn from(r: nrpc::SpeedTestResult) -> Self {
        Self {
            tag: r.outbound_tag.unwrap_or_default(),
            dl_speed: r.dl_speed.unwrap_or_default(),
            ul_speed: r.ul_speed.unwrap_or_default(),
            latency: r.latency.unwrap_or(0),
            server_name: r.server_name.unwrap_or_default(),
            server_country: r.server_country.unwrap_or_default(),
            error: r.error.unwrap_or_default(),
            cancelled: r.cancelled.unwrap_or(false),
        }
    }
}

impl From<nrpc::URLTestResp> for UrlTestResult {
    fn from(r: nrpc::URLTestResp) -> Self {
        Self {
            tag: r.outbound_tag.unwrap_or_default(),
            latency_ms: r.latency_ms.unwrap_or(0),
            error: r.error.unwrap_or_default(),
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
    /// Traffic per outbound tag, as deltas since the previous poll (the
    /// core's counters are drained on read).
    Stats {
        ups: Vec<(String, i64)>,
        downs: Vec<(String, i64)>,
    },
    /// Active connections snapshot.
    Connections(Vec<ConnectionInfo>),
    /// Subscription fetched and parsed (carries the target group id).
    SubProfiles {
        gid: i32,
        entities: Vec<ncore::model::ProxyEntity>,
        /// The `Subscription-UserInfo` header (traffic/expiry), if sent.
        info: Option<String>,
    },
    /// Subscription fetch or parse failed.
    SubFailed { gid: i32, error: String },
    /// URL test results — polled while the batch runs, then the batch's
    /// final list. The same tag may arrive twice; the values agree.
    UrlTestResults {
        seq: u64,
        results: Vec<UrlTestResult>,
    },
    /// Live progress of the outbound being speed-tested right now.
    SpeedTestProgress { seq: u64, result: SpeedTestResult },
    /// Finished speed/country test results (polled country results, then
    /// the batch's final list).
    SpeedTestResults {
        seq: u64,
        results: Vec<SpeedTestResult>,
    },
    /// A whole batch failed before testing anything (the core rejected the
    /// test config, or the connection failed).
    TestBatchFailed {
        seq: u64,
        tags: Vec<String>,
        error: String,
    },
    /// The test is over: all batches ran, or it was stopped.
    TestFinished { seq: u64 },
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

/// Handle to the running worker.
pub struct WorkerHandle {
    pub tx: Sender<Command>,
    pub rx: Receiver<Event>,
    thread: Option<std::thread::JoinHandle<()>>,
}

impl WorkerHandle {
    /// Stop tests and the core, then wait up to `timeout` for the worker to
    /// finish, so the process does not exit with the core still half-way
    /// through `Stop` (or a system proxy still set).
    pub fn shutdown(&mut self, timeout: Duration) {
        let _ = self.tx.send(Command::Shutdown);
        let Some(thread) = self.thread.take() else {
            return;
        };
        let deadline = Instant::now() + timeout;
        while !thread.is_finished() && Instant::now() < deadline {
            std::thread::sleep(Duration::from_millis(20));
        }
        if thread.is_finished() {
            let _ = thread.join();
        }
    }
}

/// Spawn the RPC worker for the core at `endpoint`.
pub fn spawn(endpoint: Endpoint) -> WorkerHandle {
    let (cmd_tx, cmd_rx) = crossbeam::channel::unbounded::<Command>();
    let (ev_tx, ev_rx) = crossbeam::channel::unbounded::<Event>();

    let thread = std::thread::spawn(move || {
        Router {
            endpoint,
            tx: ev_tx,
            stop_epoch: Arc::new(AtomicU64::new(0)),
        }
        .run(cmd_rx);
    });

    WorkerHandle {
        tx: cmd_tx,
        rx: ev_rx,
        thread: Some(thread),
    }
}

/// Dispatches commands: state-changing ones to the [`Executor`] thread (in
/// order), tests and subscription fetches to threads of their own.
struct Router {
    endpoint: Endpoint,
    tx: Sender<Event>,
    /// Bumped by every `StopTest`; a test stops before its next batch once
    /// this no longer matches the value it started with.
    stop_epoch: Arc<AtomicU64>,
}

impl Router {
    fn run(self, rx: Receiver<Command>) {
        let (core_tx, core_rx) = crossbeam::channel::unbounded::<Command>();
        let executor = {
            let tx = self.tx.clone();
            let endpoint = self.endpoint.clone();
            std::thread::spawn(move || {
                Executor {
                    endpoint,
                    core: None,
                    child: None,
                    tx,
                }
                .run(core_rx)
            })
        };

        while let Ok(cmd) = rx.recv() {
            match cmd {
                Command::UrlTest {
                    seq,
                    batches,
                    url,
                    max_concurrency,
                    timeout_ms,
                } => {
                    let test = self.test_run(seq);
                    std::thread::spawn(move || {
                        test.url_test(batches, &url, max_concurrency, timeout_ms)
                    });
                }
                Command::SpeedTest {
                    seq,
                    batches,
                    mode,
                    download_addr,
                    timeout_ms,
                    country_concurrency,
                    test_current,
                } => {
                    let test = self.test_run(seq);
                    std::thread::spawn(move || {
                        test.speed_test(SpeedTestParams {
                            batches,
                            mode,
                            download_addr,
                            timeout_ms,
                            country_concurrency,
                            test_current,
                        })
                    });
                }
                Command::StopTest => self.stop_tests(false),
                Command::UpdateSubscription { gid, url, options } => {
                    let tx = self.tx.clone();
                    std::thread::spawn(move || {
                        let ev = match ncore::sub::update_subscription(&url, &options) {
                            Ok(update) => Event::SubProfiles {
                                gid,
                                entities: update.proxies.into_iter().map(|p| p.entity).collect(),
                                info: update.user_info,
                            },
                            Err(e) => Event::SubFailed {
                                gid,
                                error: format!("{e:#}"),
                            },
                        };
                        let _ = tx.send(ev);
                    });
                }
                Command::Shutdown => {
                    // Unblock running tests first, or the core's Stop would
                    // wait for them.
                    self.stop_tests(true);
                    let _ = core_tx.send(Command::Shutdown);
                    let _ = executor.join();
                    break;
                }
                other => {
                    let _ = core_tx.send(other);
                }
            }
        }
        tracing::debug!("rpc worker exited");
    }

    /// Context for a test starting now. The stop epoch is read here, on the
    /// router thread, so a `StopTest` queued right behind the test still
    /// counts for it.
    fn test_run(&self, seq: u64) -> TestRun {
        TestRun {
            endpoint: self.endpoint.clone(),
            tx: self.tx.clone(),
            seq,
            epoch: self.stop_epoch.load(Ordering::SeqCst),
            stop_epoch: self.stop_epoch.clone(),
        }
    }

    /// Abort running tests: the core cancels its test context, which makes
    /// the blocked `Test`/`SpeedTest` calls return, and the epoch keeps the
    /// test threads from starting another batch.
    fn stop_tests(&self, wait: bool) {
        self.stop_epoch.fetch_add(1, Ordering::SeqCst);
        let endpoint = self.endpoint.clone();
        let tx = self.tx.clone();
        let job = move || match Core::connect(&endpoint).and_then(|mut c| c.stop_test()) {
            Ok(()) => {
                if !wait {
                    let _ = tx.send(Event::Log("testing stopped".into()));
                }
            }
            Err(e) => {
                if !wait {
                    let _ = tx.send(Event::Error(format!("stop test failed: {e:#}")));
                }
            }
        };
        if wait {
            job();
        } else {
            std::thread::spawn(job);
        }
    }
}

struct SpeedTestParams {
    batches: Vec<TestBatch>,
    mode: SpeedTestMode,
    download_addr: String,
    timeout_ms: i32,
    country_concurrency: i32,
    test_current: bool,
}

/// One running test, on its own thread.
struct TestRun {
    endpoint: Endpoint,
    tx: Sender<Event>,
    seq: u64,
    epoch: u64,
    stop_epoch: Arc<AtomicU64>,
}

impl TestRun {
    fn stopped(&self) -> bool {
        self.stop_epoch.load(Ordering::SeqCst) != self.epoch
    }

    fn send(&self, ev: Event) {
        let _ = self.tx.send(ev);
    }

    /// Port of `MainWindow::runURLTest`, batch by batch.
    fn url_test(self, batches: Vec<TestBatch>, url: &str, max_concurrency: i32, timeout_ms: i32) {
        for batch in batches {
            if self.stopped() {
                break;
            }
            let tags: HashSet<String> = batch.tags.iter().cloned().collect();
            let req = TestReq {
                config: Some(batch.config_json),
                outbound_tags: Some(batch.tags.clone()),
                use_default_outbound: Some(false),
                url: Some(url.to_string()),
                test_current: Some(false),
                max_concurrency: Some(max_concurrency),
                test_timeout_ms: Some(timeout_ms),
            };
            let (tx, seq) = (self.tx.clone(), self.seq);
            let result = call_with_polling(
                &self.endpoint,
                URL_TEST_POLL,
                // Results a previous test left in the core's reporter.
                |core| {
                    let _ = core.query_url_test();
                },
                |core| {
                    let Ok(resp) = core.query_url_test() else {
                        return;
                    };
                    let results: Vec<UrlTestResult> = resp
                        .results
                        .unwrap_or_default()
                        .into_iter()
                        .map(UrlTestResult::from)
                        .filter(|r| tags.contains(&r.tag))
                        .collect();
                    if !results.is_empty() {
                        let _ = tx.send(Event::UrlTestResults { seq, results });
                    }
                },
                |core| core.test(req),
            );
            match result {
                Ok(resp) => self.send(Event::UrlTestResults {
                    seq: self.seq,
                    results: resp
                        .results
                        .unwrap_or_default()
                        .into_iter()
                        .map(UrlTestResult::from)
                        .collect(),
                }),
                Err(e) => self.send(Event::TestBatchFailed {
                    seq: self.seq,
                    tags: batch.tags,
                    error: format!("{e:#}"),
                }),
            }
        }
        self.send(Event::TestFinished { seq: self.seq });
    }

    /// Port of `MainWindow::runSpeedTest`, batch by batch.
    fn speed_test(self, p: SpeedTestParams) {
        let country = p.mode == SpeedTestMode::Country;
        for batch in p.batches {
            if self.stopped() {
                break;
            }
            let tags: HashSet<String> = batch.tags.iter().cloned().collect();
            let req = nrpc::SpeedTestRequest {
                config: Some(batch.config_json),
                outbound_tags: Some(batch.tags.clone()),
                test_current: Some(p.test_current),
                use_default_outbound: Some(p.test_current),
                test_download: Some(matches!(
                    p.mode,
                    SpeedTestMode::Full | SpeedTestMode::Download
                )),
                test_upload: Some(matches!(p.mode, SpeedTestMode::Full | SpeedTestMode::Upload)),
                simple_download: Some(p.mode == SpeedTestMode::SimpleDownload),
                simple_download_addr: Some(p.download_addr.clone()),
                timeout_ms: Some(p.timeout_ms),
                only_country: Some(country),
                country_concurrency: Some(p.country_concurrency),
            };
            // `test_current` reports under whatever tag the live instance
            // resolved, so it cannot be filtered by the requested one.
            let wanted = |tag: &str| p.test_current || tags.contains(tag);
            let (tx, seq) = (self.tx.clone(), self.seq);
            let result = call_with_polling(
                &self.endpoint,
                SPEED_TEST_POLL,
                |core| {
                    if country {
                        let _ = core.query_country_test();
                    }
                },
                |core| {
                    if country {
                        let Ok(resp) = core.query_country_test() else {
                            return;
                        };
                        let results: Vec<SpeedTestResult> = resp
                            .results
                            .unwrap_or_default()
                            .into_iter()
                            .map(SpeedTestResult::from)
                            .filter(|r| wanted(&r.tag))
                            .collect();
                        if !results.is_empty() {
                            let _ = tx.send(Event::SpeedTestResults { seq, results });
                        }
                    } else {
                        // Outside a running measurement the core still hands
                        // out the previous test's last result — ignore it.
                        let Ok(resp) = core.query_speed_test() else {
                            return;
                        };
                        if resp.is_running != Some(true) {
                            return;
                        }
                        if let Some(r) = resp.result.map(SpeedTestResult::from) {
                            if wanted(&r.tag) {
                                let _ = tx.send(Event::SpeedTestProgress { seq, result: r });
                            }
                        }
                    }
                },
                |core| core.speed_test(req),
            );
            match result {
                Ok(resp) => self.send(Event::SpeedTestResults {
                    seq: self.seq,
                    results: resp
                        .results
                        .unwrap_or_default()
                        .into_iter()
                        .map(SpeedTestResult::from)
                        .collect(),
                }),
                Err(e) => self.send(Event::TestBatchFailed {
                    seq: self.seq,
                    tags: batch.tags,
                    error: format!("{e:#}"),
                }),
            }
        }
        self.send(Event::TestFinished { seq: self.seq });
    }
}

/// Run `call` on a fresh connection while `poll` runs every `interval` on a
/// second one, until `call` returns. `prepare` runs once on the polling
/// connection first. Polling is best effort: without a second connection the
/// call still runs, just without live results.
fn call_with_polling<T>(
    endpoint: &Endpoint,
    interval: Duration,
    prepare: impl FnOnce(&mut Core),
    mut poll: impl FnMut(&mut Core) + Send,
    call: impl FnOnce(&mut Core) -> anyhow::Result<T>,
) -> anyhow::Result<T> {
    let mut main = Core::connect(endpoint)?;
    let poller = Core::connect(endpoint).ok().map(|mut core| {
        prepare(&mut core);
        core
    });
    let (done_tx, done_rx) = crossbeam::channel::bounded::<()>(0);
    std::thread::scope(|s| {
        if let Some(mut core) = poller {
            // Runs until `done_tx` is dropped.
            s.spawn(move || {
                while let Err(RecvTimeoutError::Timeout) = done_rx.recv_timeout(interval) {
                    poll(&mut core);
                }
            });
        }
        let result = call(&mut main);
        drop(done_tx);
        result
    })
}

/// Runs the state-changing commands, in order, on one connection.
struct Executor {
    endpoint: Endpoint,
    core: Option<Core>,
    child: Option<std::process::Child>,
    tx: Sender<Event>,
}

impl Executor {
    fn run(&mut self, rx: Receiver<Command>) {
        while let Ok(cmd) = rx.recv() {
            let shutdown = matches!(cmd, Command::Shutdown);
            self.handle(cmd);
            if shutdown {
                break;
            }
        }
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
            Command::Connect => {
                match Core::connect(&self.endpoint).and_then(|mut c| {
                    let priv_ = c.is_privileged()?;
                    Ok((c, priv_))
                }) {
                    Ok((core, privileged)) => {
                        self.log(format!("connected to core at {}", self.endpoint));
                        self.core = Some(core);
                        self.send(Event::Connected { privileged });
                    }
                    Err(e) => self.error(format!("connect failed: {e:#}")),
                }
            }
            Command::Launch { config } => match config.launch() {
                Ok((mut child, mut core)) => {
                    self.log("core process launched".to_string());
                    self.capture_child_logs(&mut child);
                    let privileged = core.is_privileged().unwrap_or(false);
                    self.child = Some(child);
                    self.core = Some(core);
                    self.send(Event::Connected { privileged });
                }
                Err(e) => self.error(format!("core launch failed: {e:#}")),
            },
            Command::Start { config_json } => {
                let Some(core) = self.core.as_mut() else {
                    self.send(Event::StartFailed("core not connected".into()));
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
            Command::Shutdown => {
                if let Some(core) = self.core.as_mut() {
                    let _ = core.stop();
                }
                if let Some(mut child) = self.child.take() {
                    let _ = child.kill();
                    let _ = child.wait();
                }
            }
            // Routed to their own threads by the router.
            Command::UrlTest { .. }
            | Command::SpeedTest { .. }
            | Command::StopTest
            | Command::UpdateSubscription { .. } => {}
        }
    }
}
