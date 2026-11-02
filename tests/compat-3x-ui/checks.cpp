int total = 0, failed = 0;
void check(bool condition, const char *name) {
    ++total;
    if (!condition) { ++failed; std::cerr << "FAIL: " << name << '\n'; }
}
void emitConfig(const QString &name, const QJsonObject &config) {
    const auto directory = qEnvironmentVariable("NEKOBOX_TEST_CONFIG_DIR");
    if (directory.isEmpty()) return;
    QFile file(QDir(directory).filePath("fixture-" + name + ".json"));
    check(file.open(QIODevice::WriteOnly), "synthetic config file opened");
    const auto bytes = QJsonDocument(config).toJson();
    check(file.write(bytes) == bytes.size(), "synthetic config file written");
}
int main() {
    using namespace Configs;
    const QString suites = "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";
    const QString pins = QString(64, 'a') + "," + QString(64, 'b');
    const QString ech = "public.example.invalid+https://dns.example.invalid/dns-query";
    const QString extra = R"({"headers":{"X-Test":"a+b%25&?"},"xmux":{"maxConnections":3}})";
    for (int type : {TrojanVLESSBean::proxy_VLESS, TrojanVLESSBean::proxy_Trojan}) {
        auto entity = std::make_shared<ProxyEntity>();
        entity->type = type == TrojanVLESSBean::proxy_VLESS ? "vless" : "trojan";
        auto bean = std::make_shared<TrojanVLESSBean>(entity.get(), type);
        entity->storedBean = bean;
        QUrl url(entity->type + "://00000000-0000-4000-8000-000000000001@example.invalid:443");
        QUrlQuery query;
        query.addQueryItem("security", "tls");
        query.addQueryItem("type", "splithttp");
        query.addQueryItem("sni", "sni.example.invalid");
        query.addQueryItem("cs", suites);
        query.addQueryItem("pcs", pins);
        query.addQueryItem("vcn", "verify.example.invalid");
        query.addQueryItem("ech", ech);
        // A literal percent in a raw Qt query must be protected before URL encoding.
        AddQueryString(query, "extra", extra);
        url.setQuery(query);
        check(bean->TryParseLink(url.toString(QUrl::FullyEncoded)), "3x-ui link imported");
        check(bean->stream->tls_cipher_suites == suites, "cs retained");
        check(bean->stream->utlsFingerprint == "unsafe", "missing fingerprint with cs selects unsafe");
        check(bean->stream->tls_pinned_peer_cert_sha256 == pins, "multiple pins retained");
        check(bean->stream->tls_verify_peer_cert_by_name == "verify.example.invalid", "vcn retained");
        check(bean->stream->xray_ech_config == ech, "dynamic ech retained");
        check(*bean->stream->network == "xhttp", "splithttp normalized");
        check(bean->stream->xhttp_extra == extra, "opaque extra keeps plus, percent and ampersand");
        check(IsBuiltinXrayProfile(entity), "TLS extension selects Xray");
        QString error;
        auto config = BuildBuiltinXrayConfig(entity, 31234, &error);
        check(error.isEmpty() && !config.isEmpty(), "native Xray config generated");
        emitConfig(entity->type + "-xhttp", config);
        auto out = config["outbounds"].toArray()[0].toObject();
        auto settings = out["streamSettings"].toObject();
        auto tls = settings["tlsSettings"].toObject();
        check(tls["cipherSuites"].toString() == suites, "cs reaches Xray");
        check(tls["pinnedPeerCertSha256"].toString() == pins, "pcs reaches Xray");
        check(tls["verifyPeerCertByName"].toString() == "verify.example.invalid", "vcn reaches Xray");
        check(tls["echConfigList"].toString() == ech, "ech reaches Xray");
        check(!tls.contains("allowInsecure"), "removed allowInsecure never emitted");
        check(settings["xhttpSettings"].toObject()["extra"].toObject() == QJsonDocument::fromJson(extra.toUtf8()).object(), "XHTTP extra preserved exactly");
        check(config["inbounds"].toArray()[0].toObject()["listen"] == "127.0.0.1", "helper SOCKS listens on loopback");
        if (type == TrojanVLESSBean::proxy_Trojan) {
            check(out["protocol"] == "trojan", "Trojan retains its protocol");
            check(out["settings"].toObject()["servers"].toArray()[0].toObject()["password"] == bean->password, "Trojan credential reaches Xray");
        } else {
            check(out["settings"].toObject()["vnext"].toArray()[0].toObject()["users"].toArray()[0].toObject()["encryption"] == "none", "VLESS default encryption retained");
        }
        ProxyEntity reimportedEntity;
        TrojanVLESSBean reimported(&reimportedEntity, type);
        check(reimported.TryParseLink(bean->ToShareLink()), "exported link accepted");
        check(reimported.stream->tls_cipher_suites == suites && reimported.stream->tls_pinned_peer_cert_sha256 == pins && reimported.stream->tls_verify_peer_cert_by_name == bean->stream->tls_verify_peer_cert_by_name && reimported.stream->xray_ech_config == ech && reimported.stream->xhttp_extra == extra, "new TLS fields survive share-link roundtrip");
        bean->stream->utlsFingerprint = "firefox";
        config = BuildBuiltinXrayConfig(entity, 31234, &error);
        tls = config["outbounds"].toArray()[0].toObject()["streamSettings"].toObject()["tlsSettings"].toObject();
        check(tls["fingerprint"] == "firefox", "explicit fingerprint not overwritten");
        bean->stream->allow_insecure = true;
        check(BuildBuiltinXrayConfig(entity, 31234, &error).isEmpty() && !error.isEmpty(), "removed insecure flag yields error");
        bean->stream->allow_insecure = false;
        error.clear(); dataStore->skip_cert = true;
        check(BuildBuiltinXrayConfig(entity, 31234, &error).isEmpty() && !error.isEmpty(), "global insecure flag yields error");
        dataStore->skip_cert = false;
        error.clear(); bean->stream->xhttp_extra = "[1,2]";
        check(BuildBuiltinXrayConfig(entity, 31234, &error).isEmpty() && !error.isEmpty(), "non-object extra rejected");
        bean->stream->xhttp_extra = extra;
        bean->stream->security = "tls";
        bean->stream->reality_pbk = "synthetic-key";
        bean->stream->reality_sid = "0123456789abcdef";
        bean->stream->reality_pqv = "synthetic-ml-dsa-verification-key";
        error.clear();
        config = BuildBuiltinXrayConfig(entity, 31234, &error);
        auto reality = config["outbounds"].toArray()[0].toObject()["streamSettings"].toObject()["realitySettings"].toObject();
        check(reality["password"] == "synthetic-key" && !reality.contains("publicKey"), "current REALITY key name");
        check(reality["mldsa65Verify"] == bean->stream->reality_pqv, "pqv reaches Xray");
        check(reimported.TryParseLink(bean->ToShareLink()) && reimported.stream->reality_pqv == bean->stream->reality_pqv, "pqv survives roundtrip");
        // Correctly sized synthetic key material: tests configuration validation,
        // not key authenticity or a successful REALITY handshake.
        bean->stream->reality_pbk = QByteArray(32, '\1').toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        bean->stream->reality_pqv = QByteArray(1952, '\1').toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        config = BuildBuiltinXrayConfig(entity, 31234, &error);
        emitConfig(entity->type + "-reality", config);
        *bean->stream->network = "tcp";
        emitConfig(entity->type + "-reality-tcp", BuildBuiltinXrayConfig(entity, 31234, &error));
        bean->stream->reality_pbk.clear();
        bean->stream->path = "/service";
        bean->stream->host = "host.example.invalid";
        for (const auto &network : {"ws", "grpc", "httpupgrade"}) {
            *bean->stream->network = network;
            error.clear();
            config = BuildBuiltinXrayConfig(entity, 31234, &error);
            check(error.isEmpty() && !config.isEmpty(), "legacy transport supported by native Xray");
            emitConfig(entity->type + "-" + network, config);
            settings = config["outbounds"].toArray()[0].toObject()["streamSettings"].toObject();
            if (QString(network) == "grpc")
                check(settings["grpcSettings"].toObject()["serviceName"] == "/service", "gRPC service retained");
            else
                check(settings[QString(network) == "ws" ? "wsSettings" : "httpupgradeSettings"].toObject()["host"] == bean->stream->host, "transport host retained");
        }
        *bean->stream->network = "kcp";
        error.clear();
        check(BuildBuiltinXrayConfig(entity, 31234, &error).isEmpty() && !error.isEmpty(), "unsupported native transport yields explicit error");
    }
    ProxyEntity externalEntity;
    TrojanVLESSBean external(&externalEntity, TrojanVLESSBean::proxy_VLESS);
    check(external.TryParseLink("vless://fixture@example.invalid:443?security=tls&type=xhttp&extra=%7B%22headers%22%3A%7B%22X-Test%22%3A%22a%2Bb%2525%26%3F%22%7D%2C%22xmux%22%3A%7B%22maxConnections%22%3A3%7D%7D"), "external percent-encoded URI accepted");
    check(external.stream->xhttp_extra == extra, "external URI decoded exactly once");
    ProxyEntity ordinaryEntity;
    TrojanVLESSBean ordinary(&ordinaryEntity, TrojanVLESSBean::proxy_VLESS);
    check(ordinary.TryParseLink("vless://fixture@example.invalid?type=raw"), "raw alias link accepted");
    check(*ordinary.stream->network == "tcp", "raw normalized to tcp");
    check(ordinary.stream->utlsFingerprint == dataStore->utlsFingerprint, "legacy default fingerprint unchanged");
    check(ordinary.stream->tls_cipher_suites.isEmpty() && ordinary.stream->reality_pqv.isEmpty(), "ordinary links have empty new fields");
    check(!IsBuiltinXrayProfile(nullptr), "null profile handled");
    for (const auto &key : {"password", "publicKey"}) {
        auto native = QJsonDocument::fromJson(R"({"protocol":"vless","settings":{"vnext":[{"address":"example.invalid","port":443,"users":[{"id":"00000000-0000-4000-8000-000000000001"}]}]},"streamSettings":{"network":"tcp","security":"reality","realitySettings":{"serverName":"sni.example.invalid","shortId":"0123456789abcdef"}}})").object();
        auto settings = native["streamSettings"].toObject();
        auto reality = settings["realitySettings"].toObject();
        reality[key] = "fixture-key";
        settings["realitySettings"] = reality;
        native["streamSettings"] = settings;
        auto converted = Subscription::convertV2RayNToSingBox(native);
        check(converted["tls"].toObject()["reality"].toObject()["public_key"] == "fixture-key", "new and legacy REALITY JSON key names accepted");
        reality["password"] = "new-key";
        reality["publicKey"] = "old-key";
        settings["realitySettings"] = reality;
        native["streamSettings"] = settings;
        converted = Subscription::convertV2RayNToSingBox(native);
        check(converted["tls"].toObject()["reality"].toObject()["public_key"] == "new-key", "password takes precedence over legacy alias");
    }
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
