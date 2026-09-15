//! One-off tool: import profiles from a legacy nekoray config (old flat
//! JSON format with embedded bean) into the current config dir.
//!
//! Usage: cargo run -p ncore --example import_legacy -- \
//!   <legacy profiles dir> <target config dir> <gid>
//!
//! Skips profiles whose (type, addr, port) already exist in the target.

use ncore::model::ProxyEntity;
use ncore::store;

fn main() {
    let mut args = std::env::args().skip(1);
    let legacy_dir = std::path::PathBuf::from(args.next().expect("legacy dir"));
    let target = std::path::PathBuf::from(args.next().expect("target dir"));
    let gid: i32 = args.next().and_then(|s| s.parse().ok()).expect("gid");

    // Existing profiles in the target (for dedup). The airport rotates
    // hostnames, so (addr, port) is unreliable — dedup by (type, name),
    // with info entries matched by their prefix ("剩余流量：...", ...).
    let mut existing = std::collections::HashSet::new();
    let mut existing_info = Vec::new();
    for (_, path) in store::list_store_files(&target, "profiles").unwrap_or_default() {
        if let Ok(p) = store::load_proxy_entity(&path) {
            existing.insert((p.r#type.clone(), p.name.clone()));
            existing_info.push(p.name.clone());
        }
    }
    let is_dup = |ptype: &str, name: &str| {
        if existing.contains(&(ptype.to_string(), name.to_string())) {
            return true;
        }
        // Info entries: same prefix before the fullwidth colon.
        if let Some(prefix) = name.split('：').next() {
            if prefix.len() < name.len()
                && existing_info
                    .iter()
                    .any(|n| n.split('：').next() == Some(prefix))
            {
                return true;
            }
        }
        false
    };

    // Current group profile list.
    let group_path = store::get_file_path(&target, "groups", gid);
    let mut group = store::load_group(&group_path).expect("load group");

    let mut next_id = store::next_store_id(&target, "profiles");
    let mut imported = 0;
    let mut skipped = 0;

    let mut files: Vec<_> = std::fs::read_dir(&legacy_dir)
        .expect("read legacy dir")
        .filter_map(|e| e.ok().map(|e| e.path()))
        .filter(|p| p.extension().map(|e| e == "json").unwrap_or(false))
        .collect();
    files.sort();

    for path in files {
        let data = std::fs::read_to_string(&path).expect("read file");
        let old: serde_json::Value = serde_json::from_str(&data).expect("parse json");
        let bean = &old["bean"];
        let ptype = old["type"].as_str().unwrap_or("").to_string();
        let addr = bean["addr"].as_str().unwrap_or("").to_string();
        let port = bean["port"].as_i64().unwrap_or(0) as i32;
        let name = bean["name"].as_str().unwrap_or("").to_string();

        if is_dup(&ptype, &name) {
            skipped += 1;
            continue;
        }

        // Profile record (new key names).
        let mut p = ProxyEntity::new(&ptype);
        p.id = next_id;
        next_id += 1;
        p.gid = gid;
        p.name = name.clone();
        p.server_address = addr.clone();
        p.server_port = port;
        p.latency_int = old["yc"].as_i64().unwrap_or(0) as i32;
        p.traffic_dl = old["traffic"]["dl"].as_i64().unwrap_or(0);
        p.traffic_ul = old["traffic"]["ul"].as_i64().unwrap_or(0);

        // Bean: drop embedded entity fields, remap legacy key spellings.
        let mut bean = bean.clone();
        if let Some(obj) = bean.as_object_mut() {
            obj.remove("addr");
            obj.remove("port");
            obj.remove("name");
            if let Some(hop) = obj.remove("hopInterval") {
                if let Some(secs) = hop.as_i64() {
                    obj.insert("hop_interval".into(), format!("{secs}s").into());
                }
            }
        }

        store::save_proxy_entity(&target, &p).expect("save profile");
        store::save_bean_cfg(&target, p.id, &bean).expect("save bean");
        group.add_profile(p.id);
        imported += 1;
        println!("imported: {ptype} {name} ({addr}:{port}) -> id {}", p.id);
    }

    store::save_group(&target, &group).expect("save group");
    println!("done: {imported} imported, {skipped} skipped (already present)");
}
