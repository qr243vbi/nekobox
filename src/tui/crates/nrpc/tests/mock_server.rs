//! Integration test: nrpc client against a mock LibcoreService server.
//!
//! Uses the generated `LibcoreServiceSyncProcessor` to serve a few RPCs
//! on a local TCP socket and verifies the `Core` client roundtrip
//! (binary protocol, buffered transport — same as the real Go core).

use std::collections::BTreeMap;
use std::net::TcpListener;

use thrift::protocol::{TBinaryInputProtocol, TBinaryOutputProtocol};
use thrift::server::TProcessor;
use thrift::transport::{TBufferedReadTransport, TBufferedWriteTransport, TIoChannel, TTcpChannel};

use nrpc::*;

struct MockHandler;

impl LibcoreServiceSyncHandler for MockHandler {
    fn handle_enable_system_proxy(&self, _req: SystemProxy) -> thrift::Result<ErrorResp> {
        Ok(ErrorResp::new(None))
    }
    fn handle_disable_system_proxy(&self, _req: EmptyReq) -> thrift::Result<ErrorResp> {
        Ok(ErrorResp::new(None))
    }
    fn handle_is_supported(&self, req: Type) -> thrift::Result<Supported> {
        Ok(Supported::new(req.type_.is_some()))
    }
    fn handle_cache_h_t_t_p(&self, _req: CacheURLRequest) -> thrift::Result<CacheURLResult> {
        Ok(CacheURLResult::new("/tmp/cache".to_string(), false))
    }
    fn handle_i_p_test(&self, _req: IPTestRequest) -> thrift::Result<QueryIPTestResponse> {
        Ok(QueryIPTestResponse::new(vec![]))
    }
    fn handle_query_i_p_test(&self, _req: EmptyReq) -> thrift::Result<QueryIPTestResponse> {
        Ok(QueryIPTestResponse::new(vec![]))
    }
    fn handle_start(&self, req: LoadConfigReq) -> thrift::Result<ErrorResp> {
        // Fail when the config is empty, succeed otherwise.
        if req.core_config.as_deref().unwrap_or("").is_empty() {
            Ok(ErrorResp::new("empty config".to_string()))
        } else {
            Ok(ErrorResp::new(None))
        }
    }
    fn handle_stop(&self, _req: EmptyReq) -> thrift::Result<ErrorResp> {
        Ok(ErrorResp::new(None))
    }
    fn handle_check_config(&self, _req: LoadConfigReq) -> thrift::Result<ErrorResp> {
        Ok(ErrorResp::new(None))
    }
    fn handle_test(&self, _req: TestReq) -> thrift::Result<TestResp> {
        Ok(TestResp::new(vec![]))
    }
    fn handle_stop_test(&self, _req: EmptyReq) -> thrift::Result<EmptyResp> {
        Ok(EmptyResp::new(true))
    }
    fn handle_query_u_r_l_test(&self, _req: EmptyReq) -> thrift::Result<QueryURLTestResponse> {
        Ok(QueryURLTestResponse::new(vec![URLTestResp::new(
            "7".to_string(),
            123,
            "".to_string(),
        )]))
    }
    fn handle_query_stats(&self, _req: EmptyReq) -> thrift::Result<QueryStatsResp> {
        let mut ups = BTreeMap::new();
        let mut downs = BTreeMap::new();
        ups.insert("proxy".to_string(), 1024);
        downs.insert("proxy".to_string(), 2048);
        Ok(QueryStatsResp::new(ups, downs))
    }
    fn handle_list_connections(&self, _req: EmptyReq) -> thrift::Result<ListConnectionsResp> {
        Ok(ListConnectionsResp::new(vec![]))
    }
    fn handle_set_system_d_n_s(&self, _req: SetSystemDNSRequest) -> thrift::Result<EmptyResp> {
        Ok(EmptyResp::new(true))
    }
    fn handle_is_privileged(&self, _req: EmptyReq) -> thrift::Result<IsPrivilegedResponse> {
        Ok(IsPrivilegedResponse::new(true))
    }
    fn handle_speed_test(&self, _req: SpeedTestRequest) -> thrift::Result<SpeedTestResponse> {
        Ok(SpeedTestResponse::new(vec![]))
    }
    fn handle_query_speed_test(&self, _req: EmptyReq) -> thrift::Result<QuerySpeedTestResponse> {
        Ok(QuerySpeedTestResponse::new(
            SpeedTestResult::default(),
            false,
        ))
    }
    fn handle_query_country_test(&self, _req: EmptyReq) -> thrift::Result<QueryCountryTestResponse> {
        Ok(QueryCountryTestResponse::new(vec![]))
    }
    fn handle_gen_wg_key_pair(&self, _req: EmptyReq) -> thrift::Result<GenWgKeyPairResponse> {
        Ok(GenWgKeyPairResponse::new(
            "priv".to_string(),
            "pub".to_string(),
            "".to_string(),
        ))
    }
}

/// Serve connections on the listener until it is dropped.
fn serve(listener: TcpListener) {
    for stream in listener.incoming() {
        let Ok(stream) = stream else { break };
        let channel = TTcpChannel::with_stream(stream);
        let Ok((rh, wh)) = channel.split() else { break };
        let mut i_prot = TBinaryInputProtocol::new(TBufferedReadTransport::new(rh), false);
        let mut o_prot = TBinaryOutputProtocol::new(TBufferedWriteTransport::new(wh), false);
        let processor = LibcoreServiceSyncProcessor::new(MockHandler);
        // Serve until the client disconnects.
        while processor.process(&mut i_prot, &mut o_prot).is_ok() {}
    }
}

#[test]
fn test_core_roundtrip_against_mock_server() {
    let listener = TcpListener::bind("127.0.0.1:0").expect("bind mock server");
    let port = listener.local_addr().unwrap().port();
    std::thread::spawn(move || serve(listener));

    let mut core = Core::connect_tcp("127.0.0.1", port).expect("connect");

    assert!(core.is_privileged().expect("is_privileged"));

    // Start with a non-empty config succeeds.
    core.start(LoadConfigReq {
        core_config: Some("{\"outbounds\":[]}".to_string()),
        ..Default::default()
    })
    .expect("start");

    // Start with an empty config surfaces ErrorResp.error.
    let err = core
        .start(LoadConfigReq::default())
        .expect_err("empty config must fail");
    assert!(err.to_string().contains("empty config"));

    let stats = core.query_stats().expect("query_stats");
    assert_eq!(
        stats.ups.as_ref().and_then(|m| m.get("proxy")).copied(),
        Some(1024)
    );

    let url_test = core.query_url_test().expect("query_url_test");
    let results = url_test.results.unwrap_or_default();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].latency_ms, Some(123));

    core.stop().expect("stop");
}
