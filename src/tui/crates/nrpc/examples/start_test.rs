//! Debug helper: connect to a running nekobox_core and exercise RPCs.
//!
//! Usage:
//!   cargo run -p nrpc --example start_test -- <port> [config.json]
//!   cargo run -p nrpc --example start_test -- --uds <sock path> [--read-only]
//!   cargo run -p nrpc --example start_test -- --tcp <port> [--read-only]

use nrpc::{Core, LoadConfigReq};

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let read_only = args.iter().any(|a| a == "--read-only");

    let mut core = match args.first().map(String::as_str) {
        Some("--uds") => Core::connect_uds(args.get(1).expect("uds path")).expect("connect uds"),
        Some("--tcp") => Core::connect_tcp(
            "127.0.0.1",
            args.get(2).and_then(|s| s.parse().ok()).unwrap_or(19810),
        )
        .expect("connect tcp"),
        _ => Core::connect_tcp(
            "127.0.0.1",
            args.first().and_then(|s| s.parse().ok()).unwrap_or(19810),
        )
        .expect("connect tcp"),
    };

    println!("privileged: {:?}", core.is_privileged());
    println!("stats: {:?}", core.query_stats().map(|s| (s.ups, s.downs)));

    if read_only {
        println!("read-only: skipping start/stop");
        return;
    }

    let config = args
        .iter()
        .find(|a| a.ends_with(".json"))
        .map(|p| std::fs::read_to_string(p).expect("read config"))
        .unwrap_or_else(|| "{\"outbounds\":[]}".to_string());

    let req = LoadConfigReq {
        core_config: Some(config),
        ..Default::default()
    };
    match core.start(req) {
        Ok(()) => println!("start: OK"),
        Err(e) => println!("start: ERROR: {e:#}"),
    }

    println!("stats: {:?}", core.query_stats().map(|s| (s.ups, s.downs)));
    println!("stop: {:?}", core.stop());
}
