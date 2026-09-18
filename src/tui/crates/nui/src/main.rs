//! `nekobox-tui` — Terminal UI for NekoBox.
//!
//! A keyboard-driven terminal interface for managing proxy profiles,
//! groups, routing rules, and core control — drop-in compatible with the
//! Qt6 GUI (reads the same `.cfg` config directory).

use std::io;

mod app;
mod menu;
mod rpc_worker;

fn main() -> anyhow::Result<()> {
    init_tracing();

    // Parse CLI arguments
    let args = cli::parse_args();

    // Set up terminal
    crossterm::terminal::enable_raw_mode()?;
    let mut stdout = io::stdout();
    crossterm::execute!(
        stdout,
        crossterm::terminal::EnterAlternateScreen,
        crossterm::event::EnableMouseCapture
    )?;
    let backend = ratatui::backend::CrosstermBackend::new(stdout);
    let mut terminal = ratatui::Terminal::new(backend)?;
    terminal.clear()?;

    // Restore the terminal on panic so the shell isn't left in raw mode.
    let original_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        let _ = crossterm::terminal::disable_raw_mode();
        let _ = crossterm::execute!(
            io::stdout(),
            crossterm::event::DisableMouseCapture,
            crossterm::terminal::LeaveAlternateScreen
        );
        original_hook(info);
    }));

    // Run the app
    let result = app::run(&mut terminal, &args);

    // Restore terminal
    let _ = crossterm::terminal::disable_raw_mode();
    let _ = crossterm::execute!(
        terminal.backend_mut(),
        crossterm::event::DisableMouseCapture,
        crossterm::terminal::LeaveAlternateScreen
    );
    let _ = terminal.show_cursor();

    result
}

/// Log to `nekobox-tui.log` when RUST_LOG is set; discard otherwise.
/// (stdout/stderr belong to the TUI.)
fn init_tracing() {
    if std::env::var_os("RUST_LOG").is_some() {
        if let Ok(file) = std::fs::File::create("nekobox-tui.log") {
            tracing_subscriber::fmt()
                .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
                .with_writer(std::sync::Mutex::new(file))
                .with_ansi(false)
                .init();
            return;
        }
    }
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .with_writer(io::sink)
        .init();
}

mod cli {
    use clap::Parser;

    #[derive(Parser, Debug)]
    #[command(version, about = "NekoBox TUI — terminal proxy manager")]
    pub struct Args {
        /// Core RPC port (default: from nekobox.cfg, else 19810)
        #[arg(long, env = "NEKOBOX_CORE_PORT")]
        pub core_port: Option<u16>,

        /// Core RPC address (default: 127.0.0.1)
        #[arg(long, default_value = "127.0.0.1", env = "NEKOBOX_CORE_ADDRESS")]
        pub core_address: String,

        /// Path to configuration directory (default: current directory)
        #[arg(long, short = 'c', env = "NEKOBOX_CONFIG_DIR")]
        pub config_dir: Option<String>,

        /// Use a Unix socket instead of TCP (connect or, with --launch,
        /// launch the core on this socket path)
        #[arg(long, env = "NEKOBOX_CORE_UDS_PATH")]
        pub core_uds_path: Option<String>,

        /// Launch the core process automatically
        #[arg(long)]
        pub launch: bool,

        /// Core binary path (for --launch)
        #[arg(long)]
        pub core_binary: Option<String>,
    }

    pub fn parse_args() -> Args {
        Args::parse()
    }
}
