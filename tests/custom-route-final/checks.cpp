int assertions = 0, failures = 0;
void check(const QString &name, bool ok) {
    ++assertions;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << name.toStdString() << '\n';
    }
}
QJsonObject parse(const QByteArray &text) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(text, &error);
    check("fixture parses as an object", error.error == QJsonParseError::NoError && document.isObject());
    return document.object();
}
QJsonObject outbound(const QString &type, const QString &tag) {
    return {{"type", type}, {"tag", tag}};
}
QJsonValue finalValue(const QJsonObject &config) {
    return config["route"].toObject()["final"];
}
void expectFinal(const QString &name, const QJsonObject &input, const QJsonValue &expected) {
    const auto original = input;
    const auto clean = Subscription::sanitizeSingBoxConfig(input);
    check(name + ": final", finalValue(clean) == expected);
    check(name + ": source not modified", input == original);
}
int main() {
    std::cout << "Native Qt " << qVersion() << "; real sanitizer helpers and full-JSON add lambda\n";
    const QJsonArray baseOutbounds = {
        outbound("socks", "first-proxy"),
        outbound("direct", "direct"),
        outbound("block", "block"),
        outbound("dns", "dns-out"),
        QJsonObject{{"type", "selector"}, {"tag", "selection"}, {"outbounds", QJsonArray{"first-proxy", "second-proxy"}}, {"default", "second-proxy"}},
        outbound("socks", "second-proxy")
    };
    const QJsonArray rules = {QJsonObject{{"domain", QJsonArray{"example.invalid"}}, {"action", "route"}, {"outbound", "direct"}}};
    const QJsonArray ruleSets = {QJsonObject{{"tag", "synthetic"}, {"type", "inline"}, {"rules", QJsonArray{}}}};
    const QStringList finalTags = {"first-proxy", "second-proxy", "selection", "direct", "block", "dns-out", "endpoint", "not-present", "代理 🌐", " tag with spaces "};
    // All outbound positions: an explicit default must never depend on ordering.
    for (int rotation = 0; rotation < baseOutbounds.size(); ++rotation) {
        QJsonArray ordered;
        for (int i = 0; i < baseOutbounds.size(); ++i)
            ordered.append(baseOutbounds[(i + rotation) % baseOutbounds.size()]);
        for (const auto &tag : finalTags) {
            const QJsonObject route = {{"final", tag}, {"rules", rules}, {"rule_set", ruleSets}, {"default_interface", "synthetic-interface"}};
            const QJsonObject input = {{"outbounds", ordered}, {"route", route}, {"endpoints", QJsonArray{outbound("wireguard", "endpoint")}}};
            const auto clean = Subscription::sanitizeSingBoxConfig(input);
            const auto name = QString::number(rotation) + "/" + tag;
            check(name + ": explicit final retained", finalValue(clean) == tag);
            check(name + ": route rules retained", clean["route"].toObject()["rules"].toArray() == rules);
            check(name + ": rule sets retained", clean["route"].toObject()["rule_set"].toArray() == ruleSets);
            check(name + ": other route fields retained", clean["route"].toObject()["default_interface"] == "synthetic-interface");
            check(name + ": outbound graph retained", clean["outbounds"] == ordered);
            check(name + ": endpoints retained", clean["endpoints"] == input["endpoints"]);
            check(name + ": sanitizer idempotent for these inputs", Subscription::sanitizeSingBoxConfig(clean) == clean);
        }
    }
    // Existing implicit fallback is intentionally unchanged, including malformed
    // final values treated as empty by Qt. This patch is not schema validation.
    QJsonArray fallbackObs = {outbound("direct", "direct"), outbound("block", "block"), outbound("dns", "dns-out"), outbound("socks", "legacy-proxy"), outbound("socks", "later")};
    for (const QJsonValue &value : {QJsonValue(QJsonValue::Undefined), QJsonValue(""), QJsonValue(QJsonValue::Null), QJsonValue(7), QJsonValue(false), QJsonValue(QJsonArray{}), QJsonValue(QJsonObject{})}) {
        QJsonObject route;
        if (!value.isUndefined()) route["final"] = value;
        expectFinal("legacy fallback for type " + QString::number(value.type()), {{"outbounds", fallbackObs}, {"route", route}}, "legacy-proxy");
    }
    for (const QString kind : {"selector", "urltest", "socks"}) {
        expectFinal("first eligible " + kind, {{"outbounds", QJsonArray{outbound("direct", "direct"), outbound(kind, "chosen"), outbound("socks", "later")}}}, "chosen");
        expectFinal("untagged first eligible " + kind, {{"outbounds", QJsonArray{outbound("direct", "direct"), QJsonObject{{"type", kind}}, outbound("socks", "later")}}}, "proxy");
    }
    // No eligible outbound: do not invent a default or rewrite a supplied value.
    for (const QJsonArray &obs : {QJsonArray{}, QJsonArray{outbound("direct", "direct"), outbound("block", "block"), outbound("dns", "dns-out")}}) {
        const QJsonObject input = {{"outbounds", obs}};
        const auto clean = Subscription::sanitizeSingBoxConfig(input);
        check("no eligible fallback does not insert final", !clean["route"].toObject().contains("final"));
        expectFinal("no eligible fallback preserves explicit final", {{"outbounds", obs}, {"route", QJsonObject{{"final", "endpoint"}}}}, "endpoint");
    }
    // Real V2RayN converter and route conversion still use the existing fallback.
    auto legacy = parse(R"({"remarks":"legacy","outbounds":[{"protocol":"freedom","tag":"direct"},{"protocol":"socks","tag":"legacy-proxy","settings":{"servers":[{"address":"192.0.2.1","port":1080}]}}],"routing":{"domainStrategy":"AsIs","rules":[{"type":"field","domain":["domain:example.invalid"],"outboundTag":"direct"}]}})");
    auto converted = Subscription::sanitizeSingBoxConfig(legacy);
    check("legacy missing final fallback", finalValue(converted) == "legacy-proxy");
    check("legacy routing key removed", !converted.contains("routing"));
    check("legacy protocol converted", converted["outbounds"].toArray()[0].toObject()["type"] == "direct");
    check("legacy rule converted", converted["route"].toObject()["rules"].toArray()[0].toObject() == QJsonObject{{"action", "route"}, {"outbound", "direct"}, {"domain_suffix", QJsonArray{"example.invalid"}}});
    legacy["routing"] = QJsonObject{{"final", "direct"}};
    legacy["route"] = QJsonObject{{"final", "other"}};
    expectFinal("existing routing-over-route precedence retained", legacy, "direct");

    // Execute the verbatim full-config importer lambda, including the actual
    // sanitizer and Qt compact serialization, with a synthetic profile sink.
    Subscription::Importer importer;
    for (const auto &tag : finalTags) {
        auto input = parse(R"({"outbounds":[{"type":"selector","tag":"selection","outbounds":["node"]},{"type":"socks","tag":"node","server":"192.0.2.1","server_port":1080},{"type":"direct","tag":"direct"}],"route":{"final":"direct","rules":[]},"dns":{"servers":[]},"inbounds":[{"type":"mixed","listen_port":12345}]})");
        input["route"] = QJsonObject{{"final", tag}, {"rules", rules}};
        const auto before = importer.profiles.size();
        importer.add(input, "synthetic " + tag);
        check("full import adds exactly one profile", importer.profiles.size() == before + 1);
        const auto profile = importer.profiles.back();
        check("full import preserves mode", profile->storage->core == "internal-full");
        check("full import preserves name", profile->name == "synthetic " + tag);
        check("full import selector display unchanged", profile->display_type == "SELECTOR/JSON");
        const auto stored = parse(profile->storage->config_simple.toUtf8());
        check("full import serialized final retained", finalValue(stored) == tag);
        check("full import serialized rules retained", stored["route"].toObject()["rules"].toArray() == rules);
        check("full import existing inbound sanitation", stored["inbounds"].toArray().isEmpty());
    }
    check("successive imports have distinct ownership", importer.profiles.front() != importer.profiles.back());
    check("earlier serialized final survives later imports", finalValue(parse(importer.profiles.front()->storage->config_simple.toUtf8())) == finalTags.front());
    std::cout << assertions - failures << '/' << assertions << " assertions passed; " << failures << " failed\n";
    return failures ? 1 : 0;
}
