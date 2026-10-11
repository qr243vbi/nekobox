int total = 0, failed = 0;
void check(bool result, const char *name) {
    ++total;
    if (!result) { ++failed; std::cout << "FAIL " << name << '\n'; }
}
int main() {
    using namespace Configs;
    const QString configs[] = {
        "-----BEGIN ECH CONFIGS-----\nAAH+/w==\n-----END ECH CONFIGS-----\n",
        "malformed-opaque-text",
        "opaque%0A&allowInsecure=1",
        "opaque%25=+/",
        ""
    };
    for (int type : {TrojanVLESSBean::proxy_Trojan, TrojanVLESSBean::proxy_VLESS}) {
        for (const auto &config : configs) {
            for (bool enabled : {false, true}) {
                ProxyEntity originalEntity{"Synthetic profile", "example.invalid", 443};
                TrojanVLESSBean original(&originalEntity, type);
                original.password = "00000000-0000-4000-8000-000000000001";
                original.flow = "xtls-rprx-vision";
                original.encryption = "none";
                original.mux_state = 2;
                original.stream->security = "tls";
                original.stream->enable_ech = enabled;
                original.stream->ech_config = config;
                original.stream->query_server_name = "query.example.invalid";
                original.stream->sni = "sni.example.invalid";
                original.stream->alpn = "h2,http/1.1";
                original.stream->utlsFingerprint = "chrome";
                *original.stream->network = "ws";
                original.stream->path = "/test";
                original.stream->host = "host.example.invalid";
                const auto link = original.ToShareLink();
                auto query = GetQuery(QUrl(link));
                auto sharedImported = std::make_shared<V2rayStreamSettings>();
                From_Link::add_tls(sharedImported, query);
                const bool sharedExpected = enabled && !config.isEmpty();
                check(sharedImported->enable_ech == sharedExpected, "shared helper enable preserved");
                check(sharedImported->ech_config == (sharedExpected ? config : QString()), "shared helper preserves PEM and opaque text");

                check(query.hasQueryItem("ech_config") == (enabled && !config.isEmpty()), "existing ECH export format unchanged");
                check(query.hasQueryItem("query_server_name") == enabled, "existing query-name export unchanged");
                ProxyEntity importedEntity;
                TrojanVLESSBean imported(&importedEntity, type);
                check(imported.TryParseLink(link), "actual importer accepts actual exported link");
                const bool expectedEnabled = enabled && !config.isEmpty();
                check(imported.stream->enable_ech == expectedEnabled, "existing nonempty-config enable convention");
                check(imported.stream->ech_config == (expectedEnabled ? config : QString()), "PEM or opaque ECH text preserved");
                check(imported.stream->query_server_name == (expectedEnabled ? original.stream->query_server_name : QString()), "ECH query name preserved");
                check(importedEntity.name == originalEntity.name && importedEntity.serverAddress == originalEntity.serverAddress && importedEntity.serverPort == originalEntity.serverPort, "profile identity preserved");
                check(imported.password == original.password, "credential preserved");
                check(imported.stream->security == original.stream->security, "TLS mode preserved");
                check(!imported.stream->allow_insecure, "certificate verification unchanged");
                check(imported.stream->sni == original.stream->sni && imported.stream->alpn == original.stream->alpn, "SNI and ALPN preserved");
                check(imported.stream->utlsFingerprint == original.stream->utlsFingerprint, "fingerprint preserved");
                check(*imported.stream->network == *original.stream->network && imported.stream->path == original.stream->path && imported.stream->host == original.stream->host, "WebSocket transport preserved");
                check(imported.mux_state == 2, "mux disable preserved");
                if (type == TrojanVLESSBean::proxy_VLESS)
                    check(imported.flow == original.flow && imported.encryption == original.encryption, "VLESS fields preserved");
            }
        }
        // Compare the manual path with the existing shared helper, including
        // missing/empty, malformed text, and a reused stream's prior state.
        for (const auto &config : configs) {
            for (bool initial : {false, true}) {
                QUrl url(type == 0 ? "trojan://fixture@example.invalid:443" : "vless://fixture@example.invalid:443");
                QUrlQuery query;
                query.addQueryItem("security", "tls");
                query.addQueryItem("ech_config", config);
                query.addQueryItem("query_server_name", "query.example.invalid");
                url.setQuery(query);
                ProxyEntity entity;
                TrojanVLESSBean imported(&entity, type);
                auto expected = std::make_shared<V2rayStreamSettings>();
                imported.stream->enable_ech = expected->enable_ech = initial;
                imported.stream->ech_config = expected->ech_config = "previous-text";
                imported.stream->query_server_name = expected->query_server_name = "previous.invalid";
                auto parsedQuery = GetQuery(QUrl(url.toString(QUrl::FullyEncoded)));
                From_Link::add_tls(expected, parsedQuery);
                check(imported.TryParseLink(url.toString(QUrl::FullyEncoded)), "synthetic link accepted");
                check(imported.stream->enable_ech == expected->enable_ech && imported.stream->ech_config == expected->ech_config && imported.stream->query_server_name == expected->query_server_name, "manual importer matches existing shared ECH semantics");
            }
        }
        // An unrelated absent ECH field must not enable ECH or change TLS defaults.
        ProxyEntity entity;
        TrojanVLESSBean absent(&entity, type);
        check(absent.TryParseLink(type == 0 ? "trojan://fixture@example.invalid" : "vless://fixture@example.invalid"), "plain link accepted");
        check(!absent.stream->enable_ech, "absent ECH stays disabled");
        check(absent.stream->security == (type == 0 ? "tls" : ""), "protocol-specific TLS default unchanged");
    }
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
