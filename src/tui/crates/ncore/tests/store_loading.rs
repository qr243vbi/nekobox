//! Store loading smoke test: GUI-compatible .cfg files round-trip.

use ncore::model::{Group, ProxyEntity};

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

    let outbound = ncore::config::build_outbound(&profile);
    assert_eq!(outbound["type"], "shadowsocks");
    assert_eq!(outbound["password"], "secret");
}
