//! `nrpc` — Apache Thrift RPC client for `nekobox_core`.
//!
//! The Go core exposes `LibcoreService` over Thrift (TBinaryProtocol,
//! buffered transport, TCP). Bindings are generated at build time from
//! `core/server/gen/libcore.thrift` (see `build.rs`); this module provides
//! an ergonomic synchronous [`Core`] client on top of them.
//!
//! The client is synchronous — run it on a dedicated worker thread (the TUI
//! does exactly that).
//!
//! ## Usage
//!
//! ```ignore
//! let mut core = Core::connect_tcp("127.0.0.1", 19810)?;
//! let priv = core.is_privileged()?;
//! let stats = core.query_stats()?;
//! ```

use std::io::{Read, Write};
use std::time::Duration;

use thrift::protocol::{TBinaryInputProtocol, TBinaryOutputProtocol};
use thrift::transport::{
    TBufferedReadTransport, TBufferedWriteTransport, TIoChannel, TTcpChannel,
};

// Generated Thrift bindings (libcore.rs, inner attributes stripped by build.rs).
#[allow(dead_code, unused_imports, clippy::all)]
mod gen {
    include!(concat!(env!("OUT_DIR"), "/libcore_clean.rs"));
}

pub use gen::*;

// ============================================================================
// Connection
// ============================================================================

/// Client over any duplex byte stream (TCP or Unix socket).
type Client = LibcoreServiceSyncClient<
    TBinaryInputProtocol<TBufferedReadTransport<Box<dyn Read + Send>>>,
    TBinaryOutputProtocol<TBufferedWriteTransport<Box<dyn Write + Send>>>,
>;

/// Connection to nekobox_core.
///
/// Wraps the generated `LibcoreServiceSyncClient` and turns `ErrorResp.error`
/// into `anyhow` errors.
pub struct Core {
    client: Client,
}

impl Core {
    fn new_client(
        read: Box<dyn Read + Send>,
        write: Box<dyn Write + Send>,
    ) -> Self {
        // Strict write / non-strict read, matching the C++ GUI client
        // (`TBinaryProtocol` defaults) — the Go server accepts both.
        let input = TBinaryInputProtocol::new(TBufferedReadTransport::new(read), false);
        let output = TBinaryOutputProtocol::new(TBufferedWriteTransport::new(write), true);
        Self {
            client: LibcoreServiceSyncClient::new(input, output),
        }
    }

    /// Connect via TCP to the core.
    pub fn connect_tcp(addr: &str, port: u16) -> anyhow::Result<Self> {
        let mut channel = TTcpChannel::new();
        channel.open(format!("{addr}:{port}"))?;
        let (read_half, write_half) = channel.split()?;
        Ok(Self::new_client(Box::new(read_half), Box::new(write_half)))
    }

    /// Connect via Unix domain socket (Linux default for the GUI:
    /// the core is launched with `-address <sock path> -port -1`).
    #[cfg(unix)]
    pub fn connect_uds(path: &str) -> anyhow::Result<Self> {
        let read = std::os::unix::net::UnixStream::connect(path)?;
        let write = read.try_clone()?;
        Ok(Self::new_client(Box::new(read), Box::new(write)))
    }

    /// Check if the core is reachable and whether it has admin privileges.
    pub fn is_privileged(&mut self) -> anyhow::Result<bool> {
        let resp = self.client.is_privileged(EmptyReq::new(None))?;
        Ok(resp.has_privilege.unwrap_or(false))
    }

    /// Query current traffic stats.
    pub fn query_stats(&mut self) -> anyhow::Result<QueryStatsResp> {
        Ok(self.client.query_stats(EmptyReq::new(None))?)
    }

    /// Start the core with the given sing-box config.
    pub fn start(&mut self, req: LoadConfigReq) -> anyhow::Result<()> {
        check_error(self.client.start(req)?, "core start")
    }

    /// Stop the core.
    pub fn stop(&mut self) -> anyhow::Result<()> {
        check_error(self.client.stop(EmptyReq::new(None))?, "core stop")
    }

    /// Validate a sing-box config without applying it.
    pub fn check_config(&mut self, req: LoadConfigReq) -> anyhow::Result<()> {
        check_error(self.client.check_config(req)?, "config check")
    }

    /// URL test for proxy latency measurement (async on the core side —
    /// poll [`Core::query_url_test`] for results).
    pub fn test(&mut self, req: TestReq) -> anyhow::Result<TestResp> {
        Ok(self.client.test(req)?)
    }

    /// Stop an ongoing test.
    pub fn stop_test(&mut self) -> anyhow::Result<()> {
        self.client.stop_test(EmptyReq::new(None))?;
        Ok(())
    }

    /// Query pending URL test results.
    pub fn query_url_test(&mut self) -> anyhow::Result<QueryURLTestResponse> {
        Ok(self.client.query_u_r_l_test(EmptyReq::new(None))?)
    }

    /// List active connections.
    pub fn list_connections(&mut self) -> anyhow::Result<ListConnectionsResp> {
        Ok(self.client.list_connections(EmptyReq::new(None))?)
    }

    /// Set system DNS (clear = true removes rules added by the app).
    pub fn set_system_dns(&mut self, clear: bool) -> anyhow::Result<()> {
        self.client
            .set_system_d_n_s(SetSystemDNSRequest::new(clear))?;
        Ok(())
    }

    /// Enable system proxy.
    pub fn enable_system_proxy(&mut self, proxy: SystemProxy) -> anyhow::Result<()> {
        check_error(
            self.client.enable_system_proxy(proxy)?,
            "enable system proxy",
        )
    }

    /// Disable system proxy.
    pub fn disable_system_proxy(&mut self) -> anyhow::Result<()> {
        check_error(
            self.client.disable_system_proxy(EmptyReq::new(None))?,
            "disable system proxy",
        )
    }

    /// Run a speed test (async on the core side — poll
    /// [`Core::query_speed_test`] for results).
    pub fn speed_test(&mut self, req: SpeedTestRequest) -> anyhow::Result<SpeedTestResponse> {
        Ok(self.client.speed_test(req)?)
    }

    /// Query pending speed test results.
    pub fn query_speed_test(&mut self) -> anyhow::Result<QuerySpeedTestResponse> {
        Ok(self.client.query_speed_test(EmptyReq::new(None))?)
    }

    /// Query country test results.
    pub fn query_country_test(&mut self) -> anyhow::Result<QueryCountryTestResponse> {
        Ok(self.client.query_country_test(EmptyReq::new(None))?)
    }

    /// IP test — find the proxy's exit IP and country.
    pub fn ip_test(&mut self, req: IPTestRequest) -> anyhow::Result<QueryIPTestResponse> {
        Ok(self.client.i_p_test(req)?)
    }

    /// Query IP test results.
    pub fn query_ip_test(&mut self) -> anyhow::Result<QueryIPTestResponse> {
        Ok(self.client.query_i_p_test(EmptyReq::new(None))?)
    }

    /// Generate a WireGuard key pair.
    pub fn gen_wg_key_pair(&mut self) -> anyhow::Result<GenWgKeyPairResponse> {
        Ok(self.client.gen_wg_key_pair(EmptyReq::new(None))?)
    }

    /// Cache a URL (download a ruleset or other resource).
    pub fn cache_http(&mut self, req: CacheURLRequest) -> anyhow::Result<CacheURLResult> {
        Ok(self.client.cache_h_t_t_p(req)?)
    }

    /// Check if a protocol is supported.
    pub fn is_supported(&mut self, type_: &str) -> anyhow::Result<bool> {
        let resp = self.client.is_supported(Type::new(type_.to_string()))?;
        Ok(resp.ok.unwrap_or(false))
    }
}

/// Turn an `ErrorResp` into an `anyhow` error when non-empty.
fn check_error(resp: ErrorResp, op: &str) -> anyhow::Result<()> {
    match resp.error {
        Some(e) if !e.is_empty() => Err(anyhow::anyhow!("{op} failed: {e}")),
        _ => Ok(()),
    }
}

// ============================================================================
// Process launcher — nekobox_core
// ============================================================================

/// Configuration for launching the nekobox_core process.
#[derive(Debug, Clone)]
pub struct CoreConfig {
    /// Path to the nekobox_core binary (or just "nekobox_core" if in $PATH)
    pub binary: String,
    /// RPC port (sent as `-1` when `use_uds` is set)
    pub port: u16,
    /// RPC address (TCP host)
    pub address: String,
    /// Use a Unix domain socket instead of TCP — matches the GUI behaviour
    /// (`-address <sock path> -port -1`, see core/server/main.go)
    pub use_uds: bool,
    /// UDS path (when `use_uds` is set)
    pub uds_path: String,
    /// Whether to enable TUN mode (requires elevated privileges)
    pub admin: bool,
    /// Custom ruleset cache directory
    pub ruleset_cache_dir: String,
    /// Extra process path (for ExtraCore outbound)
    pub extra_process_path: String,
    /// Extra process args
    pub extra_process_args: String,
    /// Extra process config
    pub extra_process_conf: String,
    /// Extra process config directory
    pub extra_process_conf_dir: String,
    /// Suppress extra process output
    pub extra_no_out: bool,
}

impl Default for CoreConfig {
    fn default() -> Self {
        Self {
            binary: "nekobox_core".into(),
            port: 19810,
            address: "127.0.0.1".into(),
            use_uds: false,
            uds_path: "/tmp/nekobox_tui_core.sock".into(),
            admin: false,
            ruleset_cache_dir: std::env::var("NEKOBOX_RULESET_CACHE_DIRECTORY")
                .unwrap_or_default(),
            extra_process_path: String::new(),
            extra_process_args: String::new(),
            extra_process_conf: String::new(),
            extra_process_conf_dir: String::new(),
            extra_no_out: false,
        }
    }
}

impl CoreConfig {
    /// Build the command-line arguments for launching nekobox_core.
    /// Matches the C++ implementation in src/gharqad/sys/Process.cpp
    pub fn build_args(&self) -> Vec<String> {
        let (port, address) = if self.use_uds {
            // With a negative port the core treats -address as a Unix socket
            // path (core/server/main.go).
            ("-1".to_string(), self.uds_path.clone())
        } else {
            (self.port.to_string(), self.address.clone())
        };
        let mut args = vec![format!("-port={port}"), format!("-address={address}")];

        if self.admin {
            args.push("-admin".into());
        }

        // Wait for core PID
        let pid = std::process::id();
        args.push(format!("-waitpid={pid}"));

        args
    }

    /// Build environment variables for the core process.
    pub fn build_env(&self) -> Vec<(&'static str, String)> {
        let mut env = vec![
            ("InTheNameOf", "Iblis".into()),
            (
                "NEKOBOX_RULESET_CACHE_DIRECTORY",
                self.ruleset_cache_dir.clone(),
            ),
        ];

        if !self.extra_process_path.is_empty() {
            env.push(("NEKOBOX_EXTRA_PROCESS_PATH", self.extra_process_path.clone()));
        }
        if !self.extra_process_args.is_empty() {
            env.push(("NEKOBOX_EXTRA_PROCESS_ARGS", self.extra_process_args.clone()));
        }
        if !self.extra_process_conf.is_empty() {
            env.push(("NEKOBOX_EXTRA_PROCESS_CONF", self.extra_process_conf.clone()));
        }
        if !self.extra_process_conf_dir.is_empty() {
            env.push((
                "NEKOBOX_EXTRA_PROCESS_CONF_DIR",
                self.extra_process_conf_dir.clone(),
            ));
        }

        env
    }

    /// Launch the core process and wait for it to be ready.
    /// Returns the child handle and a Core client connected to it.
    ///
    /// Blocks the calling thread while polling — call from a worker thread.
    pub fn launch(self) -> anyhow::Result<(std::process::Child, Core)> {
        let mut cmd = std::process::Command::new(&self.binary);
        cmd.args(self.build_args())
            // Inherit the system environment (like the C++ GUI does with
            // QProcessEnvironment::systemEnvironment) and override our vars.
            .envs(self.build_env())
            .stdout(std::process::Stdio::piped())
            .stderr(std::process::Stdio::piped());

        tracing::info!(binary = %self.binary, "launching core");

        // On UDS, remove a stale socket so the core can bind.
        #[cfg(unix)]
        if self.use_uds {
            let _ = std::fs::remove_file(&self.uds_path);
        }

        use anyhow::Context as _;
        let child = cmd
            .spawn()
            .with_context(|| format!("failed to spawn core binary `{}`", self.binary))?;

        // Wait for core to be ready by polling the RPC endpoint.
        for attempt in 0..50 {
            std::thread::sleep(Duration::from_millis(200));
            if let Ok(mut core) = self.probe() {
                if core.is_privileged().is_ok() {
                    tracing::info!(attempt, "core is ready");
                    return Ok((child, core));
                }
            }
        }

        Err(anyhow::anyhow!("core failed to start within 10 seconds"))
    }

    /// Try to connect to the core's RPC endpoint (TCP or UDS).
    fn probe(&self) -> anyhow::Result<Core> {
        #[cfg(unix)]
        if self.use_uds {
            return Core::connect_uds(&self.uds_path);
        }
        Core::connect_tcp(&self.address, self.port)
    }
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_core_config_args() {
        let cfg = CoreConfig::default();
        let args = cfg.build_args();
        assert!(args.iter().any(|a| a.starts_with("-port=")));
        assert!(args.iter().any(|a| a.starts_with("-address=")));
        assert!(args.iter().any(|a| a.starts_with("-waitpid=")));
    }

    #[test]
    fn test_core_config_env() {
        let cfg = CoreConfig::default();
        let env = cfg.build_env();
        let keys: Vec<&str> = env.iter().map(|(k, _)| *k).collect();
        assert!(keys.contains(&"InTheNameOf"));
        assert!(keys.contains(&"NEKOBOX_RULESET_CACHE_DIRECTORY"));
    }
}
