using Configs::Data::Node;
using Configs::V2rayStreamSettings;
int checks = 0, failed = 0;
void check(bool ok, const std::string &name) { ++checks; if (!ok) { ++failed; std::cout << "FAIL " << name << '\n'; } }
void fields(const std::shared_ptr<V2rayStreamSettings> &stream, bool enabled, const QString &config, const QString &query, const std::string &name) {
    check(stream->enable_ech == enabled, name + ": enabled");
    check(stream->ech_config == config, name + ": config");
    check(stream->query_server_name == query, name + ": query_server_name");
}
std::shared_ptr<V2rayStreamSettings> imported(const QJsonObject &tls, const std::string &name) {
    auto stream = std::make_shared<V2rayStreamSettings>();
    check(Configs::From_Json::add_tls(stream, Node(QJsonObject{{"tls", tls}})), name + ": accepted");
    check(stream->security == "tls", name + ": TLS unchanged");
    return stream;
}
int main() {
    const QString pem = "-----BEGIN ECH CONFIGS-----\nSYNTHETIC-ONLY\n-----END ECH CONFIGS-----";
    const QString query = "ech.example.invalid";
    fields(imported(QJsonObject{{"ech", QJsonObject{{"enabled", true}, {"config", pem}, {"query_server_name", query}}}}, "nested string"), true, pem, query, "nested string");
    fields(imported(QJsonObject{{"ech", QJsonObject{{"enabled", true}, {"config", QJsonArray{"line one", "", "line three"}}, {"query_server_name", query}}}}, "nested array"), true, "line one\n\nline three", query, "nested array");
    fields(imported(QJsonObject{{"ech", QJsonObject{{"enabled", false}, {"config", pem}, {"query_server_name", query}}}, {"ech_config", "legacy"}, {"query_server_name", "legacy.invalid"}}, "disabled wins"), false, pem, query, "disabled wins");
    fields(imported(QJsonObject{{"ech", QJsonObject{{"config", pem}, {"query_server_name", query}}}, {"ech_config", "legacy"}}, "missing enabled"), false, pem, query, "missing enabled");
    fields(imported(QJsonObject{{"ech", QJsonObject{}}, {"ech_config", "legacy"}}, "empty nested"), false, "", "", "empty nested");
    fields(imported(QJsonObject{{"ech", QJsonObject{{"enabled", true}, {"query_server_name", query}}}}, "query only"), true, "", query, "query only");
    fields(imported(QJsonObject{{"ech", QJsonObject{{"enabled", true}, {"config", ""}}}}, "empty config"), true, "", "", "empty config");
    fields(imported(QJsonObject{{"ech", QJsonObject{{"enabled", true}, {"config", QJsonArray{}}}}}, "empty array"), true, "", "", "empty array");
    fields(imported(QJsonObject{{"ech_config", pem}, {"query_server_name", query}}, "legacy string"), true, pem, query, "legacy string");
    fields(imported(QJsonObject{{"ech_config", QJsonArray{"legacy one", "legacy two"}}, {"query_server_name", query}}, "legacy array"), true, "legacy one\nlegacy two", query, "legacy array");
    fields(imported(QJsonObject{{"query_server_name", query}}, "legacy query only"), false, "", "", "legacy query only");
    fields(imported(QJsonObject{}, "absent ECH"), false, "", "", "absent ECH");
    for (const auto &configuration : {pem, QString(""), QString("one\n\ntwo")}) {
        V2rayStreamSettings original;
        original.enable_ech = true; original.ech_config = configuration; original.query_server_name = query;
        auto stream = std::make_shared<V2rayStreamSettings>();
        auto out = original.exportEch();
#ifdef USE_REAL_QT
        out = QJsonDocument::fromJson(QJsonDocument(out).toJson()).object();
#endif
        check(Configs::From_Json::add_tls(stream, Node(out)), "export/import accepted");
        fields(stream, true, configuration, query, "actual ECH exporter roundtrip");
    }
    auto reused = std::make_shared<V2rayStreamSettings>();
    reused->enable_ech = true; reused->ech_config = "old"; reused->query_server_name = "old.invalid";
    check(Configs::From_Json::add_tls(reused, Node(QJsonObject{{"tls", QJsonObject{{"ech", QJsonObject{{"enabled", false}}}}}})), "disabled reused accepted");
    fields(reused, false, "", "", "explicit nested clears stale ECH fields");
    // Invalid nested types must reject the profile rather than silently remove ECH.
    const QJsonObject invalids[] = {
        {{"ech", QJsonValue()}}, {{"ech", true}}, {{"ech", "bad"}}, {{"ech", QJsonArray{}}},
        {{"ech", QJsonObject{{"enabled", "true"}}}}, {{"ech", QJsonObject{{"enabled", 1}}}},
        {{"ech", QJsonObject{{"enabled", QJsonValue()}}}},
        {{"ech", QJsonObject{{"enabled", true}, {"config", 42}}}},
        {{"ech", QJsonObject{{"enabled", true}, {"config", false}}}},
        {{"ech", QJsonObject{{"enabled", true}, {"config", QJsonValue()}}}},
        {{"ech", QJsonObject{{"enabled", true}, {"config", QJsonObject{}}}}},
        {{"ech", QJsonObject{{"enabled", true}, {"config", QJsonArray{"line", 7}}}}},
        {{"ech", QJsonObject{{"enabled", true}, {"query_server_name", 7}}}},
        {{"ech", QJsonObject{{"enabled", true}, {"query_server_name", QJsonValue()}}}},
        {{"ech", QJsonObject{{"enabled", true}, {"query_server_name", QJsonArray{"name"}}}}},
        {{"ech", QJsonObject{{"enabled", false}, {"config", 42}}}},
        {{"ech", QJsonObject{{"enabled", false}, {"query_server_name", false}}}},
    };
    for (auto tls : invalids) {
        tls["ech_config"] = "legacy must not win";
        auto stream = std::make_shared<V2rayStreamSettings>();
        check(!Configs::From_Json::add_tls(stream, Node(QJsonObject{{"tls", tls}})), "malformed nested ECH rejected");
        fields(stream, false, "", "", "rejected import keeps fresh ECH defaults");
        auto reused_invalid = std::make_shared<V2rayStreamSettings>();
        reused_invalid->enable_ech = true; reused_invalid->ech_config = "old"; reused_invalid->query_server_name = "old.invalid";
        reused_invalid->security = "tls"; reused_invalid->sni = "unchanged.invalid";
        check(!Configs::From_Json::add_tls(reused_invalid, Node(QJsonObject{{"tls", tls}})), "malformed reused helper rejected");
        fields(reused_invalid, true, "old", "old.invalid", "rejected helper does not partially commit ECH");
        check(reused_invalid->security == "tls", "rejected helper preserves TLS security");
        check(reused_invalid->sni == "unchanged.invalid", "rejected helper preserves SNI");
        Configs::TrustTunnelBean yaml_invalid;
        check(!yaml_invalid.TryParseYaml(Node(QJsonObject{{"tls", tls}})), "shared helper failure propagates through existing TrustTunnel YAML caller");
        for (const char *type : {"anytls", "http", "juicity", "naive", "shadowtls", "trojan", "vless", "trusttunnel", "vmess"}) {
            const QJsonObject bad{{"type", type}, {"tls", tls}};
            const QJsonObject good{{"type", type}, {"tls", QJsonObject{{"ech", QJsonObject{{"enabled", true}, {"config", pem}, {"query_server_name", query}}}}}};
            auto entity = Configs::ProfileManager::NewProxyEntity(type, true);
            check(!entity->bean()->TryParseJson(Node(bad)), std::string(type) + ": malformed import propagates false");
            Subscription::RawUpdater updater;
            int fixed_before = Subscription::fixed_entities;
            updater.updateSingBox(QJsonObject{{"outbounds", QJsonArray{bad, good}}}, "");
            check(updater.proxies.size() == 1, std::string(type) + ": updater skips rejected entity");
            check(Subscription::fixed_entities == fixed_before + 1, std::string(type) + ": no fixup for rejected entity");
            auto kept = updater.proxies.size() == 1 ? updater.proxies.front()->bean()->stream : std::make_shared<V2rayStreamSettings>();
            fields(kept, true, pem, query, std::string(type) + ": updater keeps following valid entity");
        }
    }
    for (const auto &tls : {QJsonObject{}, QJsonObject{{"ech_config", pem}, {"query_server_name", query}}}) {
        Configs::TrustTunnelBean yaml_valid;
        check(yaml_valid.TryParseYaml(Node(QJsonObject{{"tls", tls}})), "existing TrustTunnel YAML caller remains valid");
        check(yaml_valid.stream->security == "tls", "existing TrustTunnel YAML TLS retained");
        fields(yaml_valid.stream, tls["ech_config"].isString(), tls["ech_config"].toString(), tls["query_server_name"].toString(), "existing TrustTunnel YAML legacy values");
    }
    fields(imported(QJsonObject{{"ech", QJsonObject{{"enabled", true}, {"config", "  literal PEM\n\n"}, {"query_server_name", ""}}}, {"ech_config", "legacy"}}, "literal nested wins"), true, "  literal PEM\n\n", "", "import preserves literal whitespace");
    fields(imported(QJsonObject{{"ech_config", ""}}, "legacy empty"), true, "", "", "legacy empty still enables");
    fields(imported(QJsonObject{{"ech_config", QJsonValue()}}, "legacy null"), false, "", "", "legacy null still absent");
    auto no_tls = std::make_shared<V2rayStreamSettings>();
    check(Configs::From_Json::add_tls(no_tls, Node(QJsonObject{})), "absent TLS accepted");
    check(no_tls->security.isEmpty(), "absent TLS remains off");
    for (const auto &fixture : fixtures) {
        auto stream = std::make_shared<V2rayStreamSettings>();
        check(Configs::From_Json::add_tls(stream, Node(fixture.outbound)) == fixture.accepted, std::string(fixture.name) + ": JSON fixture acceptance");
        fields(stream, fixture.enabled, fixture.config, fixture.query, fixture.name);
    }
    std::cout << (failed ? "FAIL" : "PASS") << ": " << checks - failed << "/" << checks << " checks; " << failed << " failed\n";
    return failed ? 1 : 0;
}
