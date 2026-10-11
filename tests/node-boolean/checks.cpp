using Configs::Data::Node;
using Configs::Data::Tag;
int checks = 0, failed = 0;
void check(bool ok, const std::string &name) {
    ++checks;
    if (!ok) { ++failed; std::cout << "FAIL: " << name << '\n'; }
}
void truth(const Node &node, bool expected, const std::string &name) {
    check(node.toBoolean() == expected, name + ": toBoolean");
    check(node.toBool() == expected, name + ": toBool alias");
}
int main() {
    std::cout << "Qt: " << qVersion() << '\n';
    for (bool value : {false, true}) {
        const std::string label = value ? "true" : "false";
        const Node node = Node::boolean(value);
        truth(node, value, "factory boolean " + label);
        truth(Node(value), value, "constructor boolean " + label);
        check(node.getBoolean(!value) == value, "strict getter " + label);
        check(node.toNumber() == (value ? 1 : 0), "boolean number " + label);
        const QVariant variant = node.toVariant();
        check(variant.metaType().id() == QMetaType::Bool, "variant preserves bool type " + label);
        check(variant.toBool() == value, "variant preserves bool value " + label);
        const QJsonValue json = QJsonValue::fromVariant(variant);
        check(json.isBool() && json.toBool() == value, "JSON conversion preserves bool " + label);
        const auto bytes = QJsonDocument(QJsonArray{json}).toJson(QJsonDocument::Compact);
        const auto reparsed = QJsonDocument::fromJson(bytes).array().at(0);
        check(bytes == (value ? "[true]" : "[false]"), "JSON boolean bytes " + label);
        truth(Node(reparsed), value, "actual JSON constructor " + label);
    }
    struct Numeric { long double value; bool expected; const char *label; };
    const Numeric numeric[] = {
        {0, false, "zero"}, {-0.0L, false, "negative zero"}, {1, true, "one"},
        {0.125L, true, "positive fraction"}, {42, true, "positive integer"},
        {-1, false, "negative integer"}, {-0.125L, false, "negative fraction"},
        {std::numeric_limits<long double>::infinity(), true, "positive infinity"},
        {-std::numeric_limits<long double>::infinity(), false, "negative infinity"},
        {std::numeric_limits<long double>::quiet_NaN(), false, "NaN"},
    };
    for (const auto &row : numeric) {
        const auto node = Node::number(row.value);
        truth(node, row.expected, row.label);
        check(node.getBoolean(true) && !node.getBoolean(false), std::string(row.label) + ": strict getter ignores numbers");
        check(node.toVariant().metaType().id() == QMetaType::Double, std::string(row.label) + ": variant remains numeric");
    }
    struct Text { const char *value; long double number; bool expected; };
    const Text texts[] = {
        {"true", 1, true}, {"TRUE", 1, true}, {"TrUe", 1, true}, {"t-r_u-e", 1, true},
        {"false", 0, false}, {"FALSE", 0, false}, {"", 0, false}, {"garbage", 0, false},
        {" true ", 0, false}, {"yes", 0, false}, {"on", 0, false}, {"0", 0, false},
        {"-0", 0, false}, {"1", 1, true}, {"2.5", 2.5, true}, {"-1", -1, false},
        {"-0.5", -0.5, false}, {" 2 ", 2, true}, {"1e2", 100, true}, {"12junk", 0, false},
    };
    for (const auto &row : texts) {
        const auto node = Node::string(EnumFieldName(row.value));
        check(node.toNumber() == row.number, std::string("source numeric policy: ") + row.value);
        truth(node, row.expected, std::string("string: ") + row.value);
        check(node.toVariant().metaType().id() == QMetaType::QString && node.toVariant().toString() == QString::fromUtf8(row.value), std::string("variant preserves text: ") + row.value);
        check(node.getBoolean(true) && !node.getBoolean(false), std::string("strict getter ignores text: ") + row.value);
    }
    truth(Node::null(), false, "null");
    truth(Node::undefined(), false, "undefined");
    check(!Node::null().toVariant().isValid() && !Node::undefined().toVariant().isValid(), "nothing remains invalid QVariant");
    truth(Node(QJsonValue(QJsonValue::Undefined)), false, "JSON undefined");
    truth(Node(QJsonValue(QJsonValue::Null)), false, "JSON null");
    truth(Node::array(), false, "empty array");
    truth(Node(QJsonArray{false, 0}), true, "nonempty array uses count, not numeric sum");
    truth(Node::map(), false, "empty map");
    truth(Node(QJsonObject{{"enabled", false}}), true, "nonempty map uses count, not contained boolean");
    const QByteArray payload = "[true,false,0,1,-1,\"true\",null,[true,false]]";
    const Node array(QJsonDocument::fromJson(payload).array());
    const auto list = array.toVariantList();
    check(list.size() == 8 && list[0].metaType().id() == QMetaType::Bool && list[0].toBool() && !list[1].toBool(), "array conversion preserves both booleans and types");
    check(array.toVariant().toList() == list, "array toVariant delegates to list conversion");
    const auto serialized = QJsonDocument(QJsonArray::fromVariantList(list)).toJson(QJsonDocument::Compact);
    check(serialized == payload, "nested array JSON bytes roundtrip");
    const auto roundtrip = Node(QJsonDocument::fromJson(serialized).array()).values();
    truth(roundtrip.at(0), true, "array roundtrip true");
    truth(roundtrip.at(1), false, "array roundtrip false");
    truth(roundtrip.at(7).values().at(0), true, "nested array roundtrip true");
    for (bool enabled : {false, true}) {
        Configs::AbstractBean jsonBean, yamlBean;
        Configs::From_Json::add_mux_state(&jsonBean, Node(QJsonObject{{"multiplex", QJsonObject{{"enabled", enabled}}}}));
        Configs::From_Yaml::add_mux_state(&yamlBean, Node(QJsonObject{{"smux", QJsonObject{{"enabled", enabled}}}}));
        check(jsonBean.mux_state == (enabled ? 1 : 2), "JSON multiplex boolean value");
        check(yamlBean.mux_state == (enabled ? 1 : 2), "YAML smux boolean value");
        check(Configs::From_Json::parseUOT(Node(QJsonObject{{"udp_over_tcp", enabled}})) == int(enabled), "JSON UDP-over-TCP boolean");
        check(Configs::From_Json::parseUOT(Node(QJsonObject{{"uot", enabled}})) == int(enabled), "JSON direct uot boolean");
        check(Configs::From_Json::parseUOT(Node(QJsonObject{{"uot", QJsonObject{{"enabled", enabled}, {"version", 2}}}})) == (enabled ? 2 : 0), "JSON uot enabled gate and explicit version");
        check(Configs::From_Yaml::parseUOT(Node(QJsonObject{{"udp-over-tcp", enabled}})) == int(enabled), "YAML UDP-over-TCP boolean");
        auto stream = std::make_shared<Configs::V2rayStreamSettings>();
        check(Configs::From_Json::add_tls(stream, Node(QJsonObject{{"tls", QJsonObject{{"fragment", enabled}, {"record_fragment", enabled}, {"insecure", enabled}, {"ech", QJsonObject{{"enabled", enabled}}}}}})), "JSON TLS helper accepts booleans");
        check(stream->enable_tls_fragment == enabled && stream->enable_tls_record_fragment == enabled && stream->allow_insecure == enabled, "JSON TLS flags preserve booleans");
        check(stream->enable_ech == enabled, "strict ECH getter remains correct");
        stream = std::make_shared<Configs::V2rayStreamSettings>();
        check(Configs::From_Yaml::add_tls(stream, Node(QJsonObject{{"tls", enabled}, {"insecure", enabled}})), "YAML TLS helper accepts boolean");
        check(stream->security == (enabled ? "tls" : ""), "YAML TLS true reaches enabled branch");
        check(stream->allow_insecure == enabled, "YAML TLS insecure flag value");
    }
    Configs::AbstractBean absentJson, absentYaml;
    Configs::From_Json::add_mux_state(&absentJson, Node(QJsonObject{}));
    Configs::From_Yaml::add_mux_state(&absentYaml, Node(QJsonObject{}));
    check(absentJson.mux_state == 0 && absentYaml.mux_state == 0, "absent mux state remains automatic");
    check(Configs::From_Json::parseUOT(Node(QJsonObject{{"uot", 2}})) == 2, "JSON numeric uot version unchanged");
    check(Configs::From_Yaml::parseUOT(Node(QJsonObject{{"udp-over-tcp", false}, {"udp-over-tcp-version", 2}})) == 2, "YAML explicit numeric version unchanged");
    std::cout << (failed ? "FAIL" : "PASS") << ": " << checks - failed << "/" << checks << " checks; " << failed << " failed\n";
    return failed ? 1 : 0;
}
