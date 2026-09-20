//! Store loading smoke test: GUI-compatible .cfg files round-trip.

use ncore::model::{Group, ProxyEntity};

#[test]
fn test_write_read_roundtrip() {
    let dir = std::env::temp_dir().join(format!("ncore_store_test_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();

    // Profile + bean
    let mut p = ncore::model::ProxyEntity::new("shadowsocks");
    p.id = ncore::store::next_store_id(&dir, "profiles");
    p.gid = 1;
    p.name = "roundtrip".into();
    p.server_address = "rt.example.com".into();
    p.server_port = 8443;
    p.latency_int = 123;
    p.bean_cfg = Some(serde_json::json!({"method": "aes-256-gcm", "pass": "pw"}));
    ncore::store::save_proxy_entity(&dir, &p).unwrap();
    ncore::store::save_bean_cfg(&dir, p.id, p.bean_cfg.as_ref().unwrap()).unwrap();

    let loaded = ncore::store::load_proxy_entity(&ncore::store::get_file_path(&dir, "profiles", p.id)).unwrap();
    assert_eq!(loaded.name, "roundtrip");
    assert_eq!(loaded.server_address, "rt.example.com");
    assert_eq!(loaded.server_port, 8443);
    assert_eq!(loaded.latency_int, 123);
    let bean = ncore::store::load_bean_cfg(&ncore::store::get_file_path(&dir, "beans", p.id)).unwrap();
    assert_eq!(bean["method"], "aes-256-gcm");
    let outbound = ncore::config::build_outbound(&{
        let mut p2 = loaded.clone();
        p2.bean_cfg = Some(bean);
        p2
    }, false, None);
    assert_eq!(outbound["password"], "pw");

    // Group
    let mut g = ncore::model::Group::new();
    g.base.id = 1;
    g.name = "g1".into();
    g.profiles = vec![p.id];
    ncore::store::save_group(&dir, &g).unwrap();
    let loaded_g = ncore::store::load_group(&ncore::store::get_file_path(&dir, "groups", 1)).unwrap();
    assert_eq!(loaded_g.profiles, vec![p.id]);

    // DataStore
    let mut ds = ncore::model::DataStore::default();
    ds.inbound_socks_port = 12345;
    ds.test_latency_url = "http://example.com/".into();
    ncore::store::save_datastore(&dir, &ds).unwrap();
    let loaded_ds = ncore::store::load_datastore(&dir.join("nekobox.cfg")).unwrap();
    assert_eq!(loaded_ds.inbound_socks_port, 12345);
    assert_eq!(loaded_ds.test_latency_url, "http://example.com/");

    std::fs::remove_dir_all(&dir).ok();
}

/// Saving settings from the TUI must not drop the GUI-only keys the model
/// does not cover (hotkeys, window geometry, `data_store_type`, …).
#[test]
fn test_save_datastore_preserves_unknown_keys() {
    let dir = std::env::temp_dir().join(format!("ncore_preserve_test_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    std::fs::write(
        dir.join(ncore::store::DATASTORE_FILE),
        r#"{"hk_mw":"Ctrl+Alt+N","program_name":"Iblis","inbound_socks_port":2080}"#,
    )
    .unwrap();

    let mut ds = ncore::store::load_datastore(&dir.join(ncore::store::DATASTORE_FILE)).unwrap();
    assert_eq!(ds.inbound_socks_port, 2080);
    ds.inbound_socks_port = 9999;
    ncore::store::save_datastore(&dir, &ds).unwrap();

    let saved: serde_json::Value =
        serde_json::from_str(&std::fs::read_to_string(dir.join(ncore::store::DATASTORE_FILE)).unwrap())
            .unwrap();
    assert_eq!(saved["inbound_socks_port"], 9999, "modelled key updated");
    assert_eq!(saved["hk_mw"], "Ctrl+Alt+N", "unmodelled key preserved");
    assert_eq!(saved["program_name"], "Iblis", "unmodelled key preserved");

    std::fs::remove_dir_all(&dir).ok();
}

/// DNS / domain-strategy / `current_route_id` live in
/// `default_route_profile.cfg`, not `nekobox.cfg`.
#[test]
fn test_routing_file_overlays_datastore() {
    let dir = std::env::temp_dir().join(format!("ncore_routing_test_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    std::fs::write(
        dir.join(ncore::store::DATASTORE_FILE),
        r#"{"inbound_socks_port":2080}"#,
    )
    .unwrap();
    std::fs::write(
        dir.join(ncore::store::ROUTING_FILE),
        r#"{"current_route_id":4,"remote_dns":"tls://1.1.1.1","domain_strategy":"ipv4_only"}"#,
    )
    .unwrap();

    // nekobox.cfg alone leaves the routing settings at their defaults...
    let bare = ncore::store::load_datastore(&dir.join(ncore::store::DATASTORE_FILE)).unwrap();
    assert_eq!(bare.remote_dns, "tls://8.8.8.8");
    assert_eq!(bare.current_route_id, 1);

    // ...load_settings overlays the routing file on top.
    let ds = ncore::store::load_settings(&dir);
    assert_eq!(ds.inbound_socks_port, 2080);
    assert_eq!(ds.remote_dns, "tls://1.1.1.1");
    assert_eq!(ds.domain_strategy, "ipv4_only");
    assert_eq!(ds.current_route_id, 4);

    // And the write-back goes to the routing file, not nekobox.cfg.
    let mut ds = ds;
    ds.current_route_id = 7;
    ncore::store::save_settings(&dir, &ds).unwrap();
    let reloaded = ncore::store::load_settings(&dir);
    assert_eq!(reloaded.current_route_id, 7);

    std::fs::remove_dir_all(&dir).ok();
}

/// The active routing chain must reach the generated sing-box config.
#[test]
fn test_routing_chain_reaches_config() {
    use ncore::model::{RouteRule, RoutingChain, OUTBOUND_BLOCK, OUTBOUND_DIRECT};

    let mut chain = RoutingChain::new();
    chain.rules = vec![
        RouteRule {
            name: "cn-direct".into(),
            domain_suffix: vec!["cn".into()],
            outbound_id: OUTBOUND_DIRECT,
            action: "route".into(),
            ..Default::default()
        },
        RouteRule {
            name: "ads".into(),
            domain_keyword: vec!["doubleclick".into()],
            outbound_id: OUTBOUND_BLOCK,
            action: "route".into(),
            ..Default::default()
        },
    ];

    let proxy = ProxyEntity::new("shadowsocks");
    let ds = ncore::model::DataStore::default();

    let without = ncore::config::build_config(&proxy, &ds).unwrap();
    // No chain: only the GUI's prelude rules (resolve by strategy + sniff).
    let rules = without["route"]["rules"].as_array().unwrap();
    assert_eq!(rules.len(), 2);
    assert_eq!(rules[0]["action"], "resolve");
    assert_eq!(rules[1]["action"], "sniff");

    let with = ncore::config::build_config_with_route(&proxy, &ds, Some(&chain), None).unwrap();
    let rules = with["route"]["rules"].as_array().unwrap();
    assert_eq!(rules.len(), 4, "prelude + two chain rules");
    assert_eq!(rules[2]["domain_suffix"][0], "cn");
    assert_eq!(rules[2]["outbound"], "direct");
    // outbound_id -3 turns action "route" into "reject".
    assert_eq!(rules[3]["action"], "reject");
    // The block outbound the rules refer to must exist.
    let tags: Vec<&str> = with["outbounds"]
        .as_array()
        .unwrap()
        .iter()
        .map(|o| o["tag"].as_str().unwrap())
        .collect();
    assert!(tags.contains(&"block"), "got {tags:?}");
}

/// Adding a domain via the log pane edits the chain and survives a save.
#[test]
fn test_add_domain_rule_roundtrip() {
    use ncore::model::{RoutingChain, SIMPLE_ACTION_BLOCK};

    let dir = std::env::temp_dir().join(format!("ncore_chain_test_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();

    let mut chain = RoutingChain::new();
    chain.base.id = 1;
    assert!(chain.add_domain_rule("ads.example.com", SIMPLE_ACTION_BLOCK, "suffix"));
    assert!(
        !chain.add_domain_rule("ads.example.com", SIMPLE_ACTION_BLOCK, "suffix"),
        "duplicate entry is a no-op"
    );

    ncore::store::save_route_chain(&dir, &chain).unwrap();
    let loaded = ncore::store::load_route_chain(
        &ncore::store::get_file_path(&dir, "route_profiles", 1),
    )
    .unwrap();
    let rule = loaded
        .rules
        .iter()
        .find(|r| r.simple_action == SIMPLE_ACTION_BLOCK)
        .expect("block rule persisted");
    assert_eq!(rule.domain_suffix, vec!["ads.example.com"]);

    std::fs::remove_dir_all(&dir).ok();
}

#[test]
fn test_load_gui_cfg_files() {
    let profile_json = r#"{"type":"shadowsocks","name":"test-ss","serverAddress":"example.com","serverPort":443,"id":7,"gid":1,"beanCfg":{"method":"aes-256-gcm","pass":"secret"}}"#;
    let group_json = r#"{"name":"testgroup","profiles":[7]}"#;

    let profile: ProxyEntity = serde_json::from_str(profile_json).expect("profile parses");
    assert_eq!(profile.id, 7);
    assert_eq!(profile.name, "test-ss");
    assert_eq!(profile.server_address, "example.com");
    assert_eq!(profile.server_port, 443);

    let group: Group = serde_json::from_str(group_json).expect("group parses");
    assert_eq!(group.name, "testgroup");
    assert_eq!(group.profiles, vec![7]);

    let outbound = ncore::config::build_outbound(&profile, false, None);
    assert_eq!(outbound["type"], "shadowsocks");
    assert_eq!(outbound["password"], "secret");
}
