namespace go gen
namespace cpp libcore

// Messages
struct EmptyReq {
 1: bool start
}

struct EmptyResp {
 1: bool start
}

struct ErrorResp {
  1: string error
}

struct LoadConfigReq {
  1: string core_config,
  2: bool disable_stats,
  3: bool need_extra_process,
  4: string extra_process_path,
  5: string extra_process_args,
  6: string extra_process_conf,
  7: string extra_process_conf_dir,
  8: bool extra_no_out
}

struct URLTestResp {
  1: string outbound_tag,
  2: i32 latency_ms,
  3: string error
}

struct TestReq {
  1: string config,
  2: list<string> outbound_tags,
  3: bool use_default_outbound,
  4: string url,
  5: bool test_current,
  6: i32 max_concurrency,
  7: i32 test_timeout_ms
}

struct TestResp {
  1: list<URLTestResp> results
}

struct QueryStatsResp {
  1: map<string, i64> ups,
  2: map<string, i64> downs
}

struct ListConnectionsResp {
  1: list<ConnectionMetaData> connections
}

struct ConnectionMetaData {
  1: string id,
  2: i64 created_at,
  3: i64 upload,
  4: i64 download,
  5: string outbound,
  6: string network,
  7: string dest,
  8: string protocol,
  9: string domain,
  10: string process
}

struct SetSystemDNSRequest {
  1: bool clear
}

struct IsPrivilegedResponse {
  1: bool has_privilege
}

struct SpeedTestRequest {
  1: string config,
  2: list<string> outbound_tags,
  3: bool test_current,
  4: bool use_default_outbound,
  5: bool test_download,
  6: bool test_upload,
  7: bool simple_download,
  8: string simple_download_addr,
  9: i32 timeout_ms,
  10: bool only_country,
  11: i32 country_concurrency
}

struct SpeedTestResult {
  1: string dl_speed,
  2: string ul_speed,
  3: i32 latency,
  4: string outbound_tag,
  5: string error,
  6: string server_name,
  7: string server_country,
  8: bool cancelled
}

struct SpeedTestResponse {
  1: list<SpeedTestResult> results
}

struct QuerySpeedTestResponse {
  1: SpeedTestResult result,
  2: bool is_running
}

struct QueryCountryTestResponse {
  1: list<SpeedTestResult> results
}

struct QueryURLTestResponse {
  1: list<URLTestResp> results
}

service LibcoreService {
  ErrorResp Start(1: LoadConfigReq req),
  ErrorResp Stop(1: EmptyReq req),
  ErrorResp CheckConfig(1: LoadConfigReq req),
  TestResp Test(1: TestReq req),
  EmptyResp StopTest(1: EmptyReq req),
  QueryURLTestResponse QueryURLTest(1: EmptyReq req),
  QueryStatsResp QueryStats(1: EmptyReq req),
  ListConnectionsResp ListConnections(1: EmptyReq req),
  EmptyResp SetSystemDNS(1: SetSystemDNSRequest req),
  IsPrivilegedResponse IsPrivileged(1: EmptyReq req),
  SpeedTestResponse SpeedTest(1: SpeedTestRequest req),
  QuerySpeedTestResponse QuerySpeedTest(1: EmptyReq req),
  QueryCountryTestResponse QueryCountryTest(1: EmptyReq req)
}
