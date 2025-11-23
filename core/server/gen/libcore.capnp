@0xabcdefabcdefabcdef;

using Go = import "/capnp/go.capnp";
$Go.package("gen");
$Go.import("Core/gen");

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("gen");

interface LibcoreService {
    start @0 (request :LoadConfigReq) -> (response :ErrorResp);
    stop @1 () -> (response :ErrorResp);
    checkConfig @2 (request :LoadConfigReq) -> (response :ErrorResp);
    test @3 (request :TestReq) -> (response :TestResp);
    stopTest @4 () -> ();
    queryURLTest @5 () -> (response :QueryURLTestResponse);
    queryStats @6 () -> (response :QueryStatsResp);
    listConnections @7 () -> (response :ListConnectionsResp);
    setSystemDNS @8 (request :SetSystemDNSRequest) -> ();
    isPrivileged @9 () -> (response :IsPrivilegedResponse);
    speedTest @10 (request :SpeedTestRequest) -> (response :SpeedTestResponse);
    querySpeedTest @11 () -> (response :QuerySpeedTestResponse);
    queryCountryTest @12 () -> (response :QueryCountryTestResponse);
}


struct ErrorResp {
    error @0 :Text = "";
}

struct LoadConfigReq {
    coreConfig @0 :Text = "";
    disableStats @1 :Bool = false;
    needExtraProcess @2 :Bool = false;
    extraProcessPath @3 :Text = "";
    extraProcessArgs @4 :Text = "";
    extraProcessConf @5 :Text = "";
    extraProcessConfDir @6 :Text = "";
    extraNoOut @7 :Bool = false;
}

struct URLTestResp {
    outboundTag @0 :Text = "";
    latencyMs @1 :Int32 = 0;
    error @2 :Text = "";
}

struct TestReq {
    config @0 :Text = "";
    outboundTags @1 :List(Text) = [];
    useDefaultOutbound @2 :Bool = false;
    url @3 :Text = "";
    testCurrent @4 :Bool = false;
    maxConcurrency @5 :Int32 = 0;
    testTimeoutMs @6 :Int32 = 0;
}

struct TestResp {
    results @0 :List(URLTestResp) = [];
}

struct QueryStatsResp {
    ups @0 :List(KeyValue) = [];
    downs @1 :List(KeyValue) = [];
}

struct KeyValue {
    key @0 :Text;
    value @1 :Int64;
}

struct ListConnectionsResp {
    connections @0 :List(ConnectionMetaData) = [];
}

struct ConnectionMetaData {
    id @0 :Text = "";
    createdAt @1 :Int64 = 0;
    upload @2 :Int64 = 0;
    download @3 :Int64 = 0;
    outbound @4 :Text = "";
    network @5 :Text = "";
    dest @6 :Text = "";
    protocol @7 :Text = "";
    domain @8 :Text = "";
    process @9 :Text = "";
}

struct SetSystemDNSRequest {
    clear @0 :Bool = false;
}

struct IsPrivilegedResponse {
    hasPrivilege @0 :Bool = false;
}

struct SpeedTestRequest {
    config @0 :Text = "";
    outboundTags @1 :List(Text) = [];
    testCurrent @2 :Bool = false;
    useDefaultOutbound @3 :Bool = false;
    testDownload @4 :Bool = false;
    testUpload @5 :Bool = false;
    simpleDownload @6 :Bool = false;
    simpleDownloadAddr @7 :Text = "";
    timeoutMs @8 :Int32 = 0;
    onlyCountry @9 :Bool = false;
    countryConcurrency @10 :Int32 = 0;
}

struct SpeedTestResult {
    dlSpeed @0 :Text = "";
    ulSpeed @1 :Text = "";
    latency @2 :Int32 = 0;
    outboundTag @3 :Text = "";
    error @4 :Text = "";
    serverName @5 :Text = "";
    serverCountry @6 :Text = "";
    cancelled @7 :Bool = false;
}

struct SpeedTestResponse {
    results @0 :List(SpeedTestResult) = [];
}

struct QuerySpeedTestResponse {
    result @0 :SpeedTestResult;
    isRunning @1 :Bool = false;
}

struct QueryCountryTestResponse {
    results @0 :List(SpeedTestResult) = [];
}

struct QueryURLTestResponse {
    results @0 :List(URLTestResp) = [];
}
