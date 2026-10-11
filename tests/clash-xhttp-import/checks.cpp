int total = 0, failed = 0;
void check(bool result, const char *name) { ++total; if (!result) { ++failed; std::cout << "FAIL " << name << '\n'; } }
QString fixture(const QString &options, const QString &network = "xhttp", const QString &type = "vless") {
    return "proxies:\n  - name: fixture\n    type: " + type + "\n    server: example.invalid\n    port: 443\n    tls: true\n    uuid: 00000000-0000-4000-8000-000000000001\n    password: fixture\n    network: " + network + "\n    servername: sni.example.invalid\n    alpn: [h2]\n    client-fingerprint: chrome\n    skip-cert-verify: false\n" + options;
}
QJsonObject exported(const std::shared_ptr<Configs::ProxyEntity> &entity) {
    QJsonObject out{{"type", entity->type}};
    entity->value->stream->BuildStreamSettingsSingBox(&out);
    return QJsonDocument::fromJson(QJsonDocument(out).toJson()).object();
}
int main() {
    struct Case { QString options, path, host, mode; };
    const QList<Case> cases{
        {"", "", "", "auto"},
        {"    xhttp-opts: {}\n", "", "", "auto"},
        {"    xhttp-opts:\n      path: /path\n      mode: stream-up\n", "/path", "", "stream-up"},
        {"    xhttp-opts:\n      path: /path\n      host: host.example.invalid\n      mode: packet-up\n", "/path", "host.example.invalid", "packet-up"},
        {"    xhttp-opts:\n      path: /\n      host: host.example.invalid\n      mode: stream-one\n", "/", "host.example.invalid", "stream-one"},
        {"    xhttp-opts:\n      path: /encoded%2Fslash+plus?literal=1\n      mode: auto\n", "/encoded%2Fslash+plus?literal=1", "", "auto"},
        {"    xhttp-opts:\n      path: '/多言語 path'\n      host: 'unicode.example.invalid'\n", QString::fromUtf8("/多言語 path"), "unicode.example.invalid", "auto"},
        {"    xhttp-opts:\n      host: host.example.invalid\n", "", "host.example.invalid", "auto"},
        {"    xhttp-opts:\n      path: ''\n      host: ''\n      mode: ''\n", "", "", ""},
        {"    xhttp-opts: null\n", "", "", "auto"},
        {"    xhttp-opts: []\n", "", "", "auto"},
        {"    xhttp-opts: invalid\n", "", "", "auto"},
        // Characterize the issue's larger example without claiming split download support.
        {"    xhttp-opts:\n      path: /path\n      mode: stream-up\n      download-settings:\n        path: /download\n        server: download.example.invalid\n        port: 443\n        servername: download-sni.example.invalid\n", "/path", "", "stream-up"},
    };
    for (const auto &item : cases) {
        Subscription::RawUpdater updater;
        check(updater.updateClash(fixture(item.options)), "valid YAML handled");
        check(updater.proxies.size() == 1, "one VLESS profile imported");
        if (updater.proxies.size() != 1) continue;
        auto entity = updater.proxies[0]; auto stream = entity->value->stream;
        check(*stream->network == "xhttp", "network remains xhttp");
        check(stream->path == item.path, "YAML path reaches bean");
        check(stream->host == item.host, "YAML host reaches bean");
        check(stream->xhttp_mode == item.mode, "YAML mode reaches bean, absent keeps auto");
        check(entity->name == "fixture" && entity->serverAddress == "example.invalid" && entity->serverPort == 443, "identity unchanged");
        check(entity->value->password == "00000000-0000-4000-8000-000000000001", "VLESS UUID unchanged");
        const auto out = exported(entity); const auto transport = out["transport"].toObject();
        check(transport["type"] == "xhttp", "sing-box transport remains xhttp");
        check(transport["path"].toString() == item.path && transport.contains("path") == !item.path.isEmpty(), "path reaches emitted JSON");
        check(transport["host"].toString() == item.host && transport.contains("host") == !item.host.isEmpty(), "host reaches emitted JSON");
        check(transport["mode"] == item.mode, "mode reaches emitted JSON");
        check(out["packet_encoding"] == "xudp", "VLESS packet encoding unchanged");
        auto tls = out["tls"].toObject();
        check(tls["enabled"] == true && !tls.contains("insecure"), "TLS verification remains enabled");
        check(tls["server_name"] == "sni.example.invalid" && tls["alpn"].toArray() == QJsonArray{"h2"}, "SNI and ALPN unchanged");
        check(tls["utls"].toObject()["fingerprint"] == "chrome", "fingerprint unchanged");
        check(stream->xhttp_extra.isEmpty(), "broader download/extra mapping remains out of scope");
    }
    const QString xhttp = "    xhttp-opts:\n      path: /wrong-transport\n      host: wrong.example.invalid\n      mode: packet-up\n";
    // Other active transports retain their own settings, even with xhttp-opts present.
    for (const QString &type : QStringList{"vless", "trojan", "vmess"}) {
        for (const QString &network : QStringList{"ws", "grpc", "tcp"}) {
            const QString opts = network == "ws" ? "    ws-opts:\n      path: /ws\n      headers:\n        Host: ws.example.invalid\n" : network == "grpc" ? "    grpc-opts:\n      grpc-service-name: grpc-service\n" : "";
            Subscription::RawUpdater updater;
            check(updater.updateClash(fixture(xhttp + opts, network, type)), "other transport batch accepted");
            check(updater.proxies.size() == 1, "other transport profile remains imported");
            if (updater.proxies.size() != 1) continue;
            auto stream = updater.proxies[0]->value->stream;
            check(stream->path == (network == "ws" ? "/ws" : network == "grpc" ? "grpc-service" : ""), "inactive xhttp options cannot overwrite path");
            check(stream->host == (network == "ws" ? "ws.example.invalid" : ""), "inactive xhttp options cannot overwrite host");
            check(stream->xhttp_mode == "auto", "inactive xhttp options cannot overwrite mode");
        }
    }
    for (const QString &type : QStringList{"trojan", "vmess"}) {
        Subscription::RawUpdater updater;
        check(updater.updateClash(fixture(xhttp, "xhttp", type)), "unchanged non-Mihomo-XHTTP protocol import");
        check(updater.proxies.size() == 1 && updater.proxies[0]->value->stream->path.isEmpty(), "VLESS mapping does not expand to other protocols");
    }
    // Each profile owns its options; a later missing map must not inherit a prior mode.
    Subscription::RawUpdater batch;
    auto input = fixture(xhttp) + fixture("").mid(QString("proxies:\n").size());
    check(batch.updateClash(input) && batch.proxies.size() == 2, "multiple profiles imported");
    check(batch.proxies.size() == 2 && batch.proxies[1]->value->stream->xhttp_mode == "auto" && batch.proxies[1]->value->stream->path.isEmpty(), "no cross-profile options leakage");
    Subscription::RawUpdater malformed;
    check(!malformed.updateClash("proxies: ["), "real parser rejects malformed YAML");
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
