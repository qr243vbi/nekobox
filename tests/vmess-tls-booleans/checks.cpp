int total = 0, failed = 0;
void check(bool passed, const QString &label) {
    ++total;
    if (!passed) { ++failed; std::cout << "FAIL " << label.toStdString() << '\n'; }
}
QString uri(const QString &query) {
    return "vmess://00000000-0000-4000-8000-000000000001@example.invalid:443?security=tls&type=tcp&sni=sni.invalid&" + query + "#synthetic";
}
void check_json(const QJsonObject &outbound, bool expected, const QString &label) {
    const auto bytes = QJsonDocument(outbound).toJson(QJsonDocument::Compact);
    QJsonParseError error;
    const auto decoded = QJsonDocument::fromJson(bytes, &error).object();
    check(error.error == QJsonParseError::NoError && decoded == outbound, label + " JSON byte roundtrip");
    const auto tls = decoded.value("tls").toObject();
    check(tls.value("enabled").toBool(), label + " TLS enabled");
    check(tls.contains("insecure") == expected, label + " JSON insecure presence");
    check(tls.value("insecure").toBool() == expected, label + " JSON insecure value");
    if (expected) check(tls.value("insecure").isBool(), label + " JSON insecure boolean type");
    check(decoded.value("type").toString() == "vmess", label + " VMess type unchanged");
    check(decoded.value("uuid").toString() == "00000000-0000-4000-8000-000000000001", label + " UUID unchanged");
    check(decoded.value("server").toString() == "example.invalid", label + " destination unchanged");
    check(decoded.value("server_port").toInt() == 443, label + " port unchanged");
    check(tls.value("server_name").toString() == "sni.invalid", label + " SNI unchanged");
}
int main() {
    using namespace Configs;
    check(!V2rayStreamSettings().allow_insecure, "production secure default");
    check(!dataStore->skip_cert, "isolated global override disabled");
    struct Input { const char *raw; bool expected; };
    // Raw URL spellings exercise real Qt decoding. Exact values follow the
    // existing shared TLS helper; no new spelling/alias policy is introduced.
    const Input values[] = {
        {nullptr, false}, {"", false}, {"false", false}, {"0", false},
        {"true", true}, {"1", true}, {"invalid", false}, {"TRUE", false},
        {"True", false}, {"FALSE", false}, {"yes", false}, {"2", false},
        {"-1", false}, {"01", false}, {"1.0", false}, {"%20true", false},
        {"true%20", false}, {"%091", false}, {"1%0A", false}, {"+1", false},
        {"%2B1", false}, {"%31", true}, {"%74%72%75%65", true},
        {"%66alse", false}, {"%30", false}, {"%2531", false},
        {"%2574rue", false}, {"%", false}, {"%2", false}, {"%zz", false},
        {"true%00", false}, {"true%26allowInsecure%3D1", false},
    };
    for (const auto &input : values) {
        for (bool initial : {false, true}) {
            ProxyEntity entity;
            VMessBean bean(&entity);
            bean.stream->allow_insecure = initial;
            const QString query = input.raw ? "allowInsecure=" + QString(input.raw) : "";
            const QString label = "VMess allowInsecure=" + QString(input.raw ? input.raw : "<absent>") + " initial=" + QString::number(initial);
            check(bean.TryParseLink(uri(query)), label + " parses");
            check(bean.stream->allow_insecure == input.expected, label + " field");
            auto control = std::make_shared<V2rayStreamSettings>();
            control->allow_insecure = initial;
            QUrlQuery control_query(QUrl("anytls://example.invalid?" + (input.raw ? "insecure=" + QString(input.raw) : "")));
            From_Link::add_tls(control, control_query);
            check(control->allow_insecure == input.expected, label + " shared TLS reference control");
            check(bean.stream->allow_insecure == control->allow_insecure, label + " shared policy agreement");
            check_json(bean.BuildCoreObjSingBox().outbound, input.expected, label);
        }
    }
    const Input queries[] = {
        {"allowInsecure", false},
        {"allowInsecure=false&allowInsecure=1", false},
        {"allowInsecure=1&allowInsecure=false", true},
        {"allowInsecure=&allowInsecure=true", false},
        {"allowInsecure=%2531&allowInsecure=true", false},
        {"host=opaque%26allowInsecure%3D1", false},
        {"allowInsecure=%26allowInsecure%3D1", false},
    };
    for (const auto &input : queries) {
        for (bool initial : {false, true}) {
            ProxyEntity entity;
            VMessBean bean(&entity);
            bean.stream->allow_insecure = initial;
            const QString label = QString(input.raw) + " initial=" + QString::number(initial);
            check(bean.TryParseLink(uri(input.raw)), label + " parses");
            check(bean.stream->allow_insecure == input.expected, label + " first-value/isolation");
            check_json(bean.BuildCoreObjSingBox().outbound, input.expected, label);
        }
    }
    ProxyEntity reuse_entity;
    VMessBean reused(&reuse_entity);
    for (const auto &query : {"allowInsecure=1", "allowInsecure=false", "allowInsecure=true", "", "allowInsecure=1", "allowInsecure=0"}) {
        bool expected = QString(query) == "allowInsecure=1" || QString(query) == "allowInsecure=true";
        check(reused.TryParseLink(uri(query)), "reused bean parses " + QString(query));
        check(reused.stream->allow_insecure == expected, "reused bean field " + QString(query));
        check_json(reused.BuildCoreObjSingBox().outbound, expected, "reused bean " + QString(query));
    }
    for (bool value : {false, true}) {
        ProxyEntity entity;
        VMessBean bean(&entity);
        check(bean.TryParseLink(uri(value ? "allowInsecure=1" : "allowInsecure=0")), "export source parses");
        QUrlQuery exported(QUrl(bean.ToShareLink()));
        check(exported.hasQueryItem("insecure") == value, "existing shared export presence");
        if (value) check(exported.queryItemValue("insecure") == "1", "existing export true spelling");
        check(!exported.hasQueryItem("allowInsecure"), "existing export alias unchanged");
        ProxyEntity fresh_entity;
        VMessBean fresh(&fresh_entity);
        check(fresh.TryParseLink(bean.ToShareLink()), "exported link reimports");
        std::cout << "OBSERVATION exported insecure=" << value << " reimported allow_insecure=" << fresh.stream->allow_insecure << '\n';
    }
    // Base64 V2RayN JSON has a separate parser branch. Neither insecure spelling
    // is supported there; fresh beans keep the declared secure default. This
    // test does not add flag handling or alter old reused-bean semantics there.
    const QJsonValue legacy_values[] = {QJsonValue(QJsonValue::Undefined), false, true, 0, 1, "false", "true", "0", "1"};
    for (const auto &value : legacy_values) {
        for (const auto &key : {"allowInsecure", "insecure"}) {
            auto object = QJsonObject{{"id", "00000000-0000-4000-8000-000000000001"}, {"add", "example.invalid"},
                                     {"port", "443"}, {"aid", "7"}, {"ps", "legacy"}, {"tls", "tls"},
                                     {"sni", "sni.invalid"}, {"net", "tcp"}, {"scy", "aes-128-gcm"},
                                     {"path", "/literal%26&allowInsecure=1"}, {key, value}};
            ProxyEntity entity;
            VMessBean bean(&entity);
            const QString link = "vmess://" + QJsonDocument(object).toJson(QJsonDocument::Compact).toBase64();
            check(bean.TryParseLink(link), "Base64 JSON parses");
            check(!bean.stream->allow_insecure, "Base64 JSON fresh secure default");
            check(bean.aid == 7 && bean.security == "aes-128-gcm", "Base64 JSON optional fields preserved");
            check(bean.stream->path == "/literal%26&allowInsecure=1", "Base64 JSON text stays opaque");
            check_json(bean.BuildCoreObjSingBox().outbound, false, "Base64 JSON");
        }
    }
    // Global override is a deliberate independent input to the actual builder.
    ProxyEntity override_entity;
    VMessBean override_bean(&override_entity);
    check(override_bean.TryParseLink(uri("allowInsecure=false")), "override fixture parses");
    dataStore->skip_cert = true;
    check_json(override_bean.BuildCoreObjSingBox().outbound, true, "explicit isolated global override");
    dataStore->skip_cert = false;
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
