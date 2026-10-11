int total = 0, failed = 0;
void check(bool passed, const QString &label) {
    ++total;
    if (!passed) { ++failed; std::cout << "FAIL " << label.toStdString() << '\n'; }
}
QString uri(const QString &scheme, const QString &query) {
    return scheme + "://00000000-0000-4000-8000-000000000001@example.invalid:443?security=tls&type=tcp&sni=sni.invalid&" + query + "#synthetic";
}
void check_json(const QJsonObject &outbound, const QString &scheme, bool expected, const QString &label) {
    const auto bytes = QJsonDocument(outbound).toJson(QJsonDocument::Compact);
    QJsonParseError error;
    const auto decoded = QJsonDocument::fromJson(bytes, &error).object();
    check(error.error == QJsonParseError::NoError && decoded == outbound, label + " JSON byte roundtrip");
    const auto tls = decoded.value("tls").toObject();
    check(tls.value("enabled").toBool(), label + " TLS enabled");
    check(tls.contains("insecure") == expected, label + " JSON insecure presence");
    check(tls.value("insecure").toBool() == expected, label + " JSON insecure value");
    if (expected) check(tls.value("insecure").isBool(), label + " JSON boolean type");
    check(decoded.value("server").toString() == "example.invalid", label + " destination unchanged");
    check(decoded.value("server_port").toInt() == 443, label + " port unchanged");
    check(decoded.value("type").toString() == scheme, label + " protocol unchanged");
    check(tls.value("server_name").toString() == "sni.invalid", label + " SNI unchanged");
    const QString key = scheme == "trojan" || scheme == "anytls" ? "password" : "uuid";
    check(decoded.value(key).toString() == "00000000-0000-4000-8000-000000000001", label + " credential unchanged");
}
struct Input { const char *raw; bool expected; };
const Input values[] = {
    {"", false}, {"0", false}, {"false", false}, {"1", true}, {"true", true},
    {"invalid", false}, {"TRUE", false}, {"True", false}, {"yes", false},
    {"2", false}, {"-1", false}, {"01", false}, {"1.0", false},
    {"%20true", false}, {"true%20", false}, {"%091", false}, {"1%0A", false},
    {"+1", false}, {"%2B1", false}, {"%31", true}, {"%74%72%75%65", true},
    {"%30", false}, {"%66alse", false}, {"%2531", false}, {"%2574rue", false},
    {"%", false}, {"%2", false}, {"%zz", false}, {"true%00", false},
    {"true%26insecure%3D1", false}, {"%26allowInsecure%3D1", false},
};
template<typename Make>
void run_suite(const QString &scheme, Make make) {
    auto run = [&](const QString &query, bool expected, bool roundtrip = false) {
        for (bool initial : {false, true}) {
            Configs::ProxyEntity entity;
            auto bean = make(&entity);
            bean->stream->allow_insecure = initial;
            const QString label = scheme + " query=" + query + " initial=" + QString::number(initial);
            check(bean->TryParseLink(uri(scheme, query)), label + " parses");
            check(bean->stream->allow_insecure == expected, label + " field");
            check_json(bean->BuildCoreObjSingBox().outbound, scheme, expected, label);
            if (roundtrip) {
                const QString exported = bean->ToShareLink();
                const QUrlQuery exported_query{QUrl(exported)};
                check(!exported_query.hasQueryItem("allowInsecure"), label + " exporter primary absent");
                check(exported_query.hasQueryItem("insecure") == expected, label + " exporter alias presence");
                if (expected) check(exported_query.queryItemValue("insecure") == "1", label + " exporter exact positive");
                Configs::ProxyEntity fresh_entity;
                auto fresh = make(&fresh_entity);
                check(fresh->TryParseLink(exported), label + " exported URI parses");
                check(fresh->stream->allow_insecure == expected, label + " fresh reimport field");
                check_json(fresh->BuildCoreObjSingBox().outbound, scheme, expected, label + " reimport");
            }
        }
    };
    auto run_raw = [&](const QString &raw, bool expected, bool strict_valid, bool parses = true) {
        const QString label = scheme + " raw=" + raw;
        check(QUrl(raw, QUrl::StrictMode).isValid() == strict_valid, label + " strict input classification");
        for (bool initial : {false, true}) {
            Configs::ProxyEntity entity;
            auto bean = make(&entity);
            bean->stream->allow_insecure = initial;
            check(bean->TryParseLink(raw) == parses, label + " tolerant parse result preserved");
            if (!parses) continue; // Existing early rejection is unchanged.
            check(bean->stream->allow_insecure == expected, label + " raw-input flag");
            const auto outbound = bean->BuildCoreObjSingBox().outbound;
            const auto encoded = QJsonDocument(outbound).toJson(QJsonDocument::Compact);
            check(QJsonDocument::fromJson(encoded).object() == outbound, label + " raw-input JSON roundtrip");
            const auto tls = outbound.value("tls").toObject();
            check(tls.value("enabled").toBool(), label + " raw-input TLS remains enabled");
            check(tls.contains("insecure") == expected, label + " raw-input JSON flag presence");
            check(tls.value("insecure").toBool() == expected, label + " raw-input JSON flag");
        }
    };
    // Every primary value must retain strict-baseline behavior regardless of
    // alias value, position, initial state, or empty/invalid primary contents.
    for (const auto &primary : values) {
        const QString item = "allowInsecure=" + QString(primary.raw);
        run(item, primary.expected);
        for (const auto &alias : values) {
            const QString other = "insecure=" + QString(alias.raw);
            // QUrl TolerantMode treats every percent as literal in a query
            // containing a malformed percent escape. This existing behavior
            // also affects otherwise valid percent-encoded primary positives.
            const QString p = primary.raw, a = alias.raw;
            const bool escaped_positive = p == "%31" || p == "%74%72%75%65";
            const bool malformed_alias = a == "%" || a == "%2" || a == "%zz";
            const bool expected = primary.expected && !(escaped_positive && malformed_alias);
            run(item + "&" + other, expected);
            run(other + "&" + item, expected);
        }
    }
    const Input primary_queries[] = {
        {"allowInsecure", false}, {"allowInsecure&insecure=1", false},
        {"insecure=true&allowInsecure", false},
        {"allowInsecure=0&allowInsecure=1&insecure=true", false},
        {"allowInsecure=1&allowInsecure=false&insecure=0", true},
        {"allowInsecure=&allowInsecure=true&insecure=1", false},
        {"allowInsecure&allowInsecure=true&insecure=1", false},
        {"insecure=1&insecure=0&allowInsecure=false", false},
        {"insecure=0&insecure=1&allowInsecure=true", true},
        {"allow%49nsecure=false&insecure=1", false},
        {"%61llowInsecure=&insecure=1", false},
        {"%61llowInsecure=true&insecure=false", true},
        {"allowInsecure=false&allow%49nsecure=1&insecure=1", false},
        {"allow%49nsecure=true&allowInsecure=false&insecure=0", true},
        {"allowInsecure=false&%69nsecure=1", false},
        {"allowInsecure=true&%69nsecure=0", true},
        {"host=text%26insecure%3D1&allowInsecure=0", false},
        {"allowInsecure=false%26insecure%3D1", false},
    };
    for (const auto &input : primary_queries) run(input.raw, input.expected);
    // Recognized primary values retain baseline meaning even if unrelated raw
    // query/fragment syntax would fail StrictMode. The gate is alias-only.
    for (const auto &input : {Input{"allowInsecure=1&ignored=%", true},
                             Input{"allowInsecure=true&ignored= ", true},
                             Input{"allowInsecure=false&ignored=%&insecure=1", false}}) {
        run_raw(uri(scheme, input.raw), input.expected, false);
    }
    run_raw(uri(scheme, "allowInsecure=true").replace("#synthetic", "#bad%"), true, false);
#ifndef PRIMARY_ONLY
    for (const auto &alias : values) run("insecure=" + QString(alias.raw), alias.expected, true);
    const Input alias_queries[] = {
        {"", false}, {"insecure", false},
        {"insecure=0&insecure=1", false}, {"insecure=1&insecure=false", true},
        {"insecure=&insecure=true", false}, {"insecure&insecure=true", false},
        {"insecure=%2531&insecure=1", false},
        {"%69nsecure=1", true}, {"ins%65cure=%74rue", true},
        {"insecure=false&%69nsecure=1", false}, {"%69nsecure=true&insecure=0", true},
        {"%2569nsecure=1", false}, {"allow%2549nsecure=false&insecure=1", true},
        {"AllowInsecure=false&insecure=1", true}, {"allowinsecure=0&insecure=true", true},
        {"Insecure=1", false}, {"INSECURE=true", false},
        {"allow_insecure=1", false}, {"host=text%26insecure%3D1", false},
        {"sni=text%26allowInsecure%3D1", false},
        {"insecure=false%26insecure%3D1", false},
        {"insecure%3D1", false}, {"insecure%00=1", false},
        {"insecure%26allowInsecure=1", false},
    };
    for (const auto &input : alias_queries) run(input.raw, input.expected, true);
    const char *malformed[] = {"%", "%2", "%zz", "%G1", "%1G", " ", "\t", "\r", "\n"};
    for (const char *bad : malformed) {
        for (const char *positive : {"1", "true", "%31", "%74rue"}) {
            run_raw(uri(scheme, "insecure=" + QString(positive) + "&ignored=" + bad), false, false);
        }
        // A malformed escape elsewhere can hide an encoded primary key from
        // Qt. Never let that activate the newly supported insecure alias.
        run_raw(uri(scheme, "allow%49nsecure=false&ignored=" + QString(bad) + "&insecure=1"), false, false);
        run_raw(uri(scheme, "%61llowInsecure=&ignored=" + QString(bad) + "&insecure=true"), false, false);
    }
    run_raw(uri(scheme, "allow%49nsecure=false&ignored=%&insecure=1"), false, false);
    run_raw(uri(scheme, "allow%49nsecure=&ignored=%zz&insecure=true"), false, false);
    run_raw(uri(scheme, "insecure=1&ignored=%25"), true, true);
    run_raw(uri(scheme, "insecure=%31&ignored=%2525"), true, true);
    run_raw(uri(scheme, "allow%49nsecure=false&insecure=1"), false, true);
    run_raw(uri(scheme, "insecure=1&ignored=%20"), true, true);
    run_raw(uri(scheme, "insecure=1&ignored=%0A"), true, true);
    run_raw(uri(scheme, "insecure=1&ignored=é"), true, true);
    run_raw(uri(scheme, "insecure=1").replace("#synthetic", "#bad%"), false, false);
    run_raw(uri(scheme, "insecure=1").replace("#synthetic", "#bad fragment"), false, false);
    run_raw(uri(scheme, "insecure=1").replace("#synthetic", "#ok%25"), true, true);
    run_raw(uri(scheme, "insecure=1").replace("@example.invalid", "%@example.invalid"), false, false);
    run_raw(uri(scheme, "allowInsecure=1").replace("@example.invalid", "%@example.invalid"), true, false);
    run_raw(uri(scheme, "insecure=1").replace("@example.invalid", "@exam ple.invalid"), false, false, false);
    run_raw(uri(scheme, "insecure=1").replace("@example.invalid:443", "@[broken:443"), false, false, false);
    run_raw(uri(scheme, "insecure=1").replace(":443?", ":bad?"), false, false, false);
    // An empty query key cannot become a sentinel fallback source.
    run_raw(uri(scheme, "=1&ignored=%"), false, false);
    // Real primary input -> shared exporter -> fresh reimport must now retain
    // the exact selected boolean. Export remains insecure=1 or key absent.
    for (const auto &primary : values) run("allowInsecure=" + QString(primary.raw), primary.expected, true);
    Configs::ProxyEntity reuse_entity;
    auto reused = make(&reuse_entity);
    const Input sequence[] = {{"insecure=1", true}, {"", false}, {"insecure=true", true},
                             {"insecure=0", false}, {"insecure=1", true},
                             {"allowInsecure=&insecure=1", false}};
    for (const auto &input : sequence) {
        check(reused->TryParseLink(uri(scheme, input.raw)), scheme + " repeated import parses");
        check(reused->stream->allow_insecure == input.expected, scheme + " repeated import resets");
    }
    for (const char *query : {"insecure=1", "allowInsecure=1", "allowInsecure=0&insecure=1"}) {
        Configs::ProxyEntity entity;
        auto bean = make(&entity);
        // These importers infer TLS from a nonempty SNI. Remove it to make
        // this a genuinely TLS-disabled fixture under existing semantics.
        const QString input = uri(scheme, query).replace("security=tls", "security=none")
                                                  .replace("sni=sni.invalid&", "");
        check(bean->TryParseLink(input), scheme + " TLS-disabled control parses");
        check(!bean->BuildCoreObjSingBox().outbound.contains("tls"), scheme + " alias does not enable TLS");
    }
    for (const char *query : {"insecure=1", "insecure=0", "allowInsecure=false&insecure=1", "allowInsecure=1&insecure=0"}) {
        Configs::ProxyEntity entity, fresh_entity;
        auto bean = make(&entity), fresh = make(&fresh_entity);
        bean->TryParseLink(uri(scheme, query));
        auto link = bean->ToShareLink();
        fresh->TryParseLink(link);
        QJsonObject evidence{{"scheme", scheme}, {"input", query}, {"selected", bean->stream->allow_insecure},
                             {"exported", link}, {"reimported", fresh->stream->allow_insecure},
                             {"outbound", fresh->BuildCoreObjSingBox().outbound}};
        std::cout << "OBSERVATION " << QJsonDocument(evidence).toJson(QJsonDocument::Compact).toStdString() << '\n';
    }
#endif
}
int main() {
    using namespace Configs;
    check(!V2rayStreamSettings().allow_insecure, "production secure default");
    check(!dataStore->skip_cert, "isolated global override disabled");
    run_suite("trojan", [](ProxyEntity *e) { return std::make_unique<TrojanVLESSBean>(e, TrojanVLESSBean::proxy_Trojan); });
    run_suite("vless", [](ProxyEntity *e) { return std::make_unique<TrojanVLESSBean>(e, TrojanVLESSBean::proxy_VLESS); });
    run_suite("vmess", [](ProxyEntity *e) { return std::make_unique<VMessBean>(e); });
#ifndef PRIMARY_ONLY
    // Shared AnyTLS remains alias-only, not migrated to primary precedence.
    for (const auto &input : {Input{"insecure=1&allowInsecure=0", true}, Input{"insecure=0&allowInsecure=1", false}, Input{"allowInsecure=1", false}}) {
        ProxyEntity entity;
        AnyTLSBean bean(&entity);
        check(bean.TryParseLink(uri("anytls", input.raw)), "AnyTLS control parses");
        check(bean.stream->allow_insecure == input.expected, "AnyTLS unchanged alias policy");
        check_json(bean.BuildCoreObjSingBox().outbound, "anytls", input.expected, "AnyTLS control");
    }
#endif
    check(!dataStore->skip_cert, "global override remains disabled");
    std::cout << "TLS URL alias checks: " << total << " checks, " << failed << " failures\n";
    return failed ? 1 : 0;
}
