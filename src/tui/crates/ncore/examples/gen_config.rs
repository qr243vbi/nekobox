//! Debug helper: build a sing-box config for a profile from a config dir.
//!
//! Usage: cargo run -p ncore --example gen_config -- <config dir> <profile id>

use ncore::store;

fn main() {
    let mut args = std::env::args().skip(1);
    let base = std::path::PathBuf::from(args.next().expect("config dir"));
    let id: i32 = args.next().and_then(|s| s.parse().ok()).expect("profile id");

    let mut proxy = store::load_proxy_entity(&store::get_file_path(&base, "profiles", id))
        .expect("load profile");
    let bean_path = store::get_file_path(&base, "beans", id);
    if bean_path.exists() {
        proxy.bean_cfg = store::load_bean_cfg(&bean_path).ok();
    }
    // Both halves of the GUI's settings, then the routing chain it has active.
    let ds = store::load_settings(&base);
    let chain = store::list_store_files(&base, "route_profiles")
        .unwrap_or_default()
        .into_iter()
        .filter_map(|(_, path)| store::load_route_chain(&path).ok())
        .find(|c| c.base.id == ds.current_route_id);
    eprintln!(
        "routing profile: {} ({} rules)",
        chain.as_ref().map_or("<none>", |c| c.chain_name.as_str()),
        chain.as_ref().map_or(0, |c| c.rules.len()),
    );

    let config = ncore::config::build_config_with_route(&proxy, &ds, chain.as_ref(), None)
        .expect("build config");
    println!("{}", serde_json::to_string_pretty(&config).unwrap());
}
