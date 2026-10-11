int total = 0, failed = 0;
void check(bool result, const QString &name) {
    ++total;
    if (!result) { ++failed; std::cout << "FAIL " << name.toStdString() << '\n'; }
}

QJsonArray fixtures;
QJsonObject exported(Configs::V2rayStreamSettings &stream, const QString &type = "vless") {
    QJsonObject out{{"type", type}};
    stream.BuildStreamSettingsSingBox(&out);
    // Real Qt serialization and reparse, not an emulated JSON value.
    return QJsonDocument::fromJson(QJsonDocument(out).toJson()).object();
}

void xhttp(const QString &name, const QString &extra, int from, int to,
           const QString &error = "", const QString &mode = "auto",
           const QString &type = "vless") {
    Configs::V2rayStreamSettings stream;
    *stream.network = "xhttp";
    *stream.packet_encoding = "xudp";
    stream.xhttp_extra = extra;
    stream.xhttp_mode = mode;
    stream.path = QString::fromUtf8("/多言語%2F+path?literal=1");
    stream.host = "host.example.invalid";
    stream.security = "tls";
    stream.sni = "sni.example.invalid";
    stream.alpn = "h2,http/1.1";
    stream.utlsFingerprint = "chrome";
    const auto out = exported(stream, type);
    const auto transport = out["transport"].toObject();
    check(transport["type"] == "xhttp", name + ": type");
    check(transport["path"] == stream.path, name + ": path");
    check(transport["host"] == stream.host, name + ": host");
    check(transport["mode"] == mode, name + ": mode");
    check(transport["x_padding_bytes"].toObject() == QJsonObject{{"from", from}, {"to", to}}, name + ": padding");
    check(stream.xhttp_extra == extra, name + ": no profile mutation");
    check(out["tls"].toObject() == QJsonObject{
        {"enabled", true}, {"server_name", stream.sni},
        {"alpn", QJsonArray{"h2", "http/1.1"}},
        {"utls", QJsonObject{{"enabled", true}, {"fingerprint", "chrome"}}}
    }, name + ": TLS unchanged");
    check((type == "vless" || type == "vmess") ? out["packet_encoding"] == "xudp" : !out.contains("packet_encoding"), name + ": packet encoding");
    check(exported(stream, type) == out, name + ": repeated emission");
    fixtures.append(QJsonObject{{"name", name}, {"outbound", out}, {"from", from}, {"to", to}, {"error", error}});
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    for (const auto &type : QStringList{"vless", "vmess", "trojan"}) {
        for (const auto &mode : QStringList{"auto", "packet-up", "stream-up", "stream-one", ""}) {
            xhttp(type + "/" + mode + "/omitted", "", 100, 1000, "", mode, type);
        }
    }
    xhttp("empty-object", "{}", 100, 1000);
    xhttp("whitespace-object", "  { }  ", 100, 1000);
    // Preserve the pre-existing non-object/malformed-extra behavior.
    xhttp("null-extra", "null", 100, 1000);
    xhttp("array-extra", "[]", 100, 1000);
    xhttp("malformed-extra", "{", 100, 1000);
    xhttp("unknown-option", R"({"futureOption":12})", 100, 1000);
    for (const auto &key : QStringList{"xPaddingBytes", "x_padding_bytes", "X_PADDING_BYTES"}) {
        xhttp(key + "/number", "{\"" + key + "\":128}", 128, 128);
        xhttp(key + "/string", "{\"" + key + "\":\"200-800\"}", 200, 800);
        xhttp(key + "/fixed-string", "{\"" + key + "\":\"512\"}", 512, 512);
        xhttp(key + "/object", "{\"" + key + "\":{\"from\":300,\"to\":900}}", 300, 900);
        for (const auto &value : QStringList{"0", "null", "false", "[]", "{}", "\"bad\""}) {
            xhttp(key + "/invalid/" + value, "{\"" + key + "\":" + value + "}", 0, 0, "x_padding_bytes cannot be disabled");
        }
    }
    xhttp("negative", R"({"xPaddingBytes":-5})", -5, -5, "x_padding_bytes cannot be disabled");
    xhttp("zero-from", R"({"xPaddingBytes":{"from":0,"to":100}})", 0, 100, "x_padding_bytes cannot be disabled");
    xhttp("zero-to", R"({"xPaddingBytes":{"from":0,"to":0}})", 0, 0, "x_padding_bytes cannot be disabled");
    xhttp("reversed", R"({"xPaddingBytes":{"from":900,"to":100}})", 900, 100, "invalid range");
    xhttp("reversed-string", R"({"xPaddingBytes":"900-100"})", 900, 100, "invalid range");
    xhttp("invalid-mode-omitted-padding", "{}", 100, 1000, "unsupported mode: invalid", "invalid");
    xhttp("invalid-mode-explicit-padding", R"({"xPaddingBytes":321})", 321, 321, "unsupported mode: invalid", "invalid");
    xhttp("other-options", R"({"noGRPCHeader":true,"noSSEHeader":true,"headers":{"X-Test":"kept"},"scMaxEachPostBytes":"1000-2000","xPaddingKey":"custom","xPaddingMethod":"tokenish","xPaddingPlacement":"header"})", 100, 1000);
    const auto other = fixtures.last().toObject()["outbound"].toObject()["transport"].toObject();
    check(other["no_grpc_header"] == true && other["no_sse_header"] == true, "other booleans preserved");
    check(other["headers"].toObject() == QJsonObject{{"X-Test", "kept"}}, "extra headers preserved");
    check(other["sc_max_each_post_bytes"].toObject() == QJsonObject{{"from", 1000}, {"to", 2000}}, "other ranges preserved");
    check(other["x_padding_key"] == "custom" && other["x_padding_method"] == "tokenish" && other["x_padding_placement"] == "header", "other padding properties preserved");
    xhttp("invalid-placement", R"({"xPaddingPlacement":"wrong"})", 100, 1000, "unsupported padding placement: wrong");
    xhttp("invalid-method", R"({"xPaddingMethod":"wrong"})", 100, 1000, "unsupported padding method: wrong");
    // The synthetic record after all explicit values must still get the default.
    xhttp("no-cross-profile-leakage", "{}", 100, 1000);
    for (const auto &network : QStringList{"tcp", "ws", "http", "grpc", "httpupgrade", "quic", "kcp", "", " "}) {
        Configs::V2rayStreamSettings stream;
        *stream.network = network;
        stream.xhttp_extra = R"({"xPaddingBytes":777})";
        const auto out = exported(stream);
        const auto transport = out["transport"].toObject();
        check(!transport.contains("x_padding_bytes"), network + ": no padding leakage");
        if (network.trimmed().isEmpty() || network == "tcp") {
            check(!out.contains("transport"), network + ": no transport");
        } else {
            check(transport["type"] == network, network + ": own transport");
            fixtures.append(QJsonObject{{"name", "non-xhttp/" + network}, {"outbound", out}, {"error", network == "kcp" ? "unknown transport type: kcp" : ""}});
        }
    }
    Configs::V2rayStreamSettings naive;
    *naive.network = "xhttp";
    naive.security = "tls";
    naive.xhttp_extra = R"({"xPaddingBytes":777})";
    const auto naiveOut = exported(naive, "naive");
    check(!naiveOut.contains("transport"), "naive skips XHTTP");
    check(naiveOut["tls"].toObject()["enabled"] == true, "naive keeps TLS");
    QFile file(argv[1]);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(fixtures).toJson()) < 0) return 2;
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << " fixtures=" << fixtures.size() << '\n';
    return failed ? 1 : 0;
}
