int total = 0, failed = 0;
void check(bool passed, const QString &label) {
    ++total;
    if (!passed) { ++failed; std::cout << "FAIL " << label.toStdString() << '\n'; }
}
QString uri(const QString &scheme, const QString &query) {
    return scheme + "://synthetic-password@example.invalid:443?security=tls&type=tcp&sni=sni.invalid&alpn=h2&" + query + "#synthetic";
}
void check_json(const QJsonObject &outbound, bool expected, const QString &label) {
    const auto bytes = QJsonDocument(outbound).toJson(QJsonDocument::Compact);
    QJsonParseError error;
    const auto decoded = QJsonDocument::fromJson(bytes, &error).object();
    check(error.error == QJsonParseError::NoError && decoded == outbound, label + " JSON byte roundtrip");
    const auto tls = decoded.value("tls").toObject();
    check(tls.value("enabled").toBool(), label + " TLS remains enabled");
    check(tls.contains("insecure") == expected, label + " JSON insecure presence");
    check(tls.value("insecure").toBool() == expected, label + " JSON insecure value");
    if (expected) check(tls.value("insecure").isBool(), label + " JSON insecure boolean type");
    check(decoded.value("server").toString() == "example.invalid", label + " destination unchanged");
    check(decoded.value("server_port").toInt() == 443, label + " port unchanged");
    check(tls.value("server_name").toString() == "sni.invalid", label + " SNI unchanged");
    check(tls.value("alpn").toArray() == QJsonArray{"h2"}, label + " ALPN unchanged");
}
int main() {
    using namespace Configs;
    check(!V2rayStreamSettings().allow_insecure, "production secure default");
    check(!dataStore->skip_cert, "isolated global override disabled");
    // These exact-match expectations follow From_Link::add_tls. Cases are raw
    // URL query text, exercising real Qt parsing, not prebuilt query objects.
    struct Input { const char *raw; bool expected; };
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
        const QString value = input.raw ? input.raw : "<absent>";
        for (bool initial : {false, true}) {
            // AnyTLS is an independent full-parser positive/reference control.
            ProxyEntity control_entity;
            AnyTLSBean control(&control_entity);
            control.stream->allow_insecure = initial;
            const QString control_query = input.raw ? "insecure=" + QString(input.raw) : "";
            const QString control_label = "AnyTLS insecure=" + value + " initial=" + QString::number(initial);
            check(control.TryParseLink(uri("anytls", control_query)), control_label + " parses");
            check(control.stream->allow_insecure == input.expected, control_label + " field");
            check_json(control.BuildCoreObjSingBox().outbound, input.expected, control_label);
            for (int type : {TrojanVLESSBean::proxy_Trojan, TrojanVLESSBean::proxy_VLESS}) {
                ProxyEntity entity;
                TrojanVLESSBean bean(&entity, type);
                const QString scheme = bean.type();
                bean.stream->allow_insecure = initial;
                const QString query = input.raw ? "allowInsecure=" + QString(input.raw) : "";
                const QString label = scheme + " allowInsecure=" + value + " initial=" + QString::number(initial);
                check(bean.TryParseLink(uri(scheme, query)), label + " parses");
                check(bean.stream->allow_insecure == input.expected, label + " field");
                check(bean.stream->allow_insecure == control.stream->allow_insecure, label + " agrees with shared semantics");
                check_json(bean.BuildCoreObjSingBox().outbound, input.expected, label);
            }
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
    for (int type : {TrojanVLESSBean::proxy_Trojan, TrojanVLESSBean::proxy_VLESS}) {
        for (const auto &input : queries) {
            for (bool initial : {false, true}) {
                ProxyEntity entity;
                TrojanVLESSBean bean(&entity, type);
                bean.stream->allow_insecure = initial;
                const QString label = bean.type() + " " + input.raw + " initial=" + QString::number(initial);
                check(bean.TryParseLink(uri(bean.type(), input.raw)), label + " parses");
                check(bean.stream->allow_insecure == input.expected, label + " first-value/isolation");
                check_json(bean.BuildCoreObjSingBox().outbound, input.expected, label);
            }
        }
        // Exercise repeated imports into one actual bean, including secure reset.
        ProxyEntity entity;
        TrojanVLESSBean reused(&entity, type);
        check(reused.TryParseLink(uri(reused.type(), "allowInsecure=1")) && reused.stream->allow_insecure, reused.type() + " reuse enables");
        check(reused.TryParseLink(uri(reused.type(), "allowInsecure=false")) && !reused.stream->allow_insecure, reused.type() + " reuse false resets");
        check(reused.TryParseLink(uri(reused.type(), "allowInsecure=true")) && reused.stream->allow_insecure, reused.type() + " reuse re-enables");
        check(reused.TryParseLink(uri(reused.type(), "")) && !reused.stream->allow_insecure, reused.type() + " reuse absent resets");
        // Shared exporter format is unchanged by this narrowly scoped value fix.
        for (bool value : {false, true}) {
            ProxyEntity original_entity;
            TrojanVLESSBean original(&original_entity, type);
            check(original.TryParseLink(uri(original.type(), value ? "allowInsecure=1" : "allowInsecure=0")), original.type() + " export source parses");
            const QUrlQuery exported(QUrl(original.ToShareLink()));
            check(exported.hasQueryItem("insecure") == value, original.type() + " shared export presence");
            if (value) check(exported.queryItemValue("insecure") == "1", original.type() + " shared export true spelling");
            check(!exported.hasQueryItem("allowInsecure"), original.type() + " existing export alias unchanged");
        }
    }
    // Global skip_cert remains a distinct override in the real JSON builder.
    ProxyEntity override_entity;
    TrojanVLESSBean override_bean(&override_entity, TrojanVLESSBean::proxy_Trojan);
    check(override_bean.TryParseLink(uri("trojan", "allowInsecure=false")), "override fixture parses");
    dataStore->skip_cert = true;
    check_json(override_bean.BuildCoreObjSingBox().outbound, true, "explicit isolated global override");
    dataStore->skip_cert = false;
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
