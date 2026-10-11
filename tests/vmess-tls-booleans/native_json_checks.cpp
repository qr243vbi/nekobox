int total = 0, failed = 0;
void check(bool value, const QString &label) {
    ++total;
    if (!value) { ++failed; std::cout << "FAIL " << label.toStdString() << '\n'; }
}
int main() {
    using namespace Configs;
    for (const auto &value : {QJsonValue(QJsonValue::Undefined), QJsonValue(false), QJsonValue(true)}) {
        for (bool initial : {false, true}) {
            const QJsonObject input{{"type", "vmess"}, {"server", "example.invalid"}, {"server_port", 443},
                                    {"uuid", "00000000-0000-4000-8000-000000000001"}, {"alter_id", 7},
                                    {"tls", QJsonObject{{"enabled", true}, {"server_name", "sni.invalid"}, {"insecure", value}}}};
            const auto parsed = QJsonDocument::fromJson(QJsonDocument(input).toJson()).object();
            ProxyEntity entity;
            VMessBean bean(&entity);
            bean.stream->allow_insecure = initial;
            check(bean.TryParseJson(Data::Node(parsed)), "native JSON parser accepts boolean/default fixture");
            const auto output = QJsonDocument::fromJson(QJsonDocument(bean.BuildCoreObjSingBox().outbound).toJson()).object();
            const auto tls = output.value("tls").toObject();
            check(tls.value("enabled").toBool(), "native JSON TLS stays enabled");
            check(output.value("server_port").toInt() == 443 && output.value("alter_id").toInt() == 7, "native JSON numeric fields retained");
            check(tls.value("server_name").toString() == "sni.invalid", "native JSON SNI retained");
            check(tls.value("insecure").toBool() == bean.stream->allow_insecure, "native JSON emitted flag matches imported field");
            if (!value.toBool()) {
                check(!bean.stream->allow_insecure && !tls.contains("insecure"), "native JSON false/default remains secure");
            } else {
                std::cout << "OBSERVATION native JSON insecure=true initial=" << initial << " imported=" << bean.stream->allow_insecure << " emitted=" << tls.value("insecure").toBool() << '\n';
#ifdef EXPECT_NATIVE_JSON_TRUE
                check(bean.stream->allow_insecure && tls.value("insecure").isBool() && tls.value("insecure").toBool(), "combined Node fix preserves native JSON true");
#endif
            }
        }
    }
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
