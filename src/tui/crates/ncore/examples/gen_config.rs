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
    let ds = store::load_datastore(&base.join("nekobox.cfg")).unwrap_or_default();

    let config = ncore::config::build_config(&proxy, &ds).expect("build config");
    println!("{}", serde_json::to_string_pretty(&config).unwrap());
}
