int total = 0, failed = 0;
void check(bool result, const char *name) { ++total; if (!result) { ++failed; std::cout << "FAIL " << name << '\n'; } }
QString fixture(const QString &type, const QString &ech, bool tls = true) {
    return "proxies:\n  - name: fixture\n    type: " + type + "\n    server: example.invalid\n    port: 443\n    tls: " + (tls ? "true" : "false") + "\n    uuid: 00000000-0000-4000-8000-000000000001\n    password: fixture\n    network: ws\n    sni: sni.example.invalid\n    alpn: [h2, http/1.1]\n    client-fingerprint: chrome\n    skip-cert-verify: false\n    ws-opts:\n      path: /test\n      headers:\n        Host: host.example.invalid\n" + ech;
}
int main() {
    const QString config = "AAH+/w==";
    const QString pem = "-----BEGIN ECH CONFIGS-----\n" + config + "\n-----END ECH CONFIGS-----\n";
    for (const QString &type : QStringList{"http", "trojan", "vless", "vmess", "anytls", "shadowtls"}) {
        struct Case { QString yaml; bool enabled; QString config, query; };
        QList<Case> cases{
            {"", false, "", ""},
            {"    ech-opts: {}\n", false, "", ""},
            {"    ech-opts:\n      enable: true\n      config: " + config + "\n      query-server-name: query.example.invalid\n", true, pem, "query.example.invalid"},
            {"    ech-opts:\n      enable: false\n      config: " + config + "\n      query-server-name: query.example.invalid\n", false, pem, "query.example.invalid"},
            {"    ech-opts:\n      enable: true\n      config: ''\n      query-server-name: query.example.invalid\n", true, "", "query.example.invalid"},
            {"    ech-opts:\n      enable: true\n", true, "", ""},
            {"    ech-opts:\n      config: " + config + "\n", false, pem, ""},
            {"    ech-opts:\n      enable: true\n      config: |\n        AAH+\n        /w==\n", true, pem, ""},
            {"    ech-opts:\n      enable: false\n      config: invalid-base64!\n", false, "-----BEGIN ECH CONFIGS-----\ninvalid-base64!\n-----END ECH CONFIGS-----\n", ""},
        };
        for (const auto &item : cases) {
            Subscription::RawUpdater updater;
            check(updater.updateClash(fixture(type, item.yaml)), "valid YAML accepted");
            check(updater.proxies.size() == 1, "one valid profile imported");
            if (updater.proxies.size() != 1) continue;
            const auto entity = updater.proxies[0]; const auto stream = entity->value->stream;
            check(stream->enable_ech == item.enabled, "explicit ECH enabled/disabled/missing semantics");
            check(stream->ech_config == item.config, "base64 binary safely wrapped as PEM");
            check(stream->query_server_name == item.query, "nested ECH query name preserved");
            check(stream->security == "tls" && !stream->allow_insecure, "TLS verification unchanged");
            check(stream->sni == "sni.example.invalid" && stream->alpn == "h2,http/1.1", "ordinary TLS fields unchanged");
            check(entity->name == "fixture" && entity->serverAddress == "example.invalid" && entity->serverPort == 443, "profile identity unchanged");
        }
        for (const QString &bad : QStringList{
            "    ech-opts: null\n", "    ech-opts: []\n", "    ech-opts: invalid\n",
            "    ech-opts:\n      enable: invalid\n", "    ech-opts:\n      enable: null\n",
            "    ech-opts:\n      enable: true\n      config: null\n", "    ech-opts:\n      enable: true\n      config: []\n",
            "    ech-opts:\n      enable: true\n      config: invalid-base64!\n", "    ech-opts:\n      enable: true\n      config: '===='\n",
            "    ech-opts:\n      enable: true\n      config: AA\n", "    ech-opts:\n      enable: true\n      config: 'AA H+/w=='\n",
            "    ech-opts:\n      enable: true\n      query-server-name: []\n", "    ech-opts:\n      query-server-name: null\n"
        }) {
            auto input = fixture(type, bad);
            input += fixture(type, "").mid(QString("proxies:\n").size());
            Subscription::RawUpdater updater;
            check(updater.updateClash(input), "batch with malformed ECH still processes following profile");
            check(updater.proxies.size() == 1, "malformed ECH profile rejected without downgrading");
        }
    }
    for (const QString &type : QStringList{"http", "vmess"}) {
        Subscription::RawUpdater updater;
        check(updater.updateClash(fixture(type, "    ech-opts:\n      enable: true\n", false)), "non-TLS profile accepted");
        check(updater.proxies.size() == 1 && !updater.proxies[0]->value->stream->enable_ech && updater.proxies[0]->value->stream->security.isEmpty(), "ECH cannot enable a non-TLS profile");
    }
    Subscription::RawUpdater malformed;
    check(!malformed.updateClash("proxies: ["), "malformed YAML rejected by real parser");
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
