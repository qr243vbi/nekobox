using Configs::ProxyEntity;
using Configs::QUICBean;
int checks = 0, failures = 0;
void equal(const QString &actual, const QString &expected, const QString &label) {
    ++checks;
    if (actual != expected) {
        ++failures;
        if (failures <= 24)
            std::cout << "FAIL " << label.toStdString() << "\n expected "
                      << expected.toStdString() << "\n actual   " << actual.toStdString() << '\n';
    }
}

int main() {
    // Fixed goldens demonstrate the regression without invoking any importer.
    ProxyEntity entity;
    entity.serverAddress = "example.invalid";
    entity.serverPort = 8443;
    QUICBean bean(&entity, QUICBean::proxy_Hysteria2);
    bean.password = "synthetic-auth";
    bean.serverPorts = {"443:445"};
    equal(bean.ToShareLink(), "hy2://synthetic-auth@example.invalid:443-445", "queryless range golden");
    entity.name = "synthetic";
    equal(bean.ToShareLink(), "hy2://synthetic-auth@example.invalid:443-445#synthetic", "fragment-only range golden");
    bean.sni = "sni.invalid";
    equal(bean.ToShareLink(), "hy2://synthetic-auth@example.invalid:443-445?sni=sni.invalid#synthetic", "query range golden");
    bean.serverPorts.clear();
    equal(bean.ToShareLink(), "hy2://synthetic-auth@example.invalid:8443?sni=sni.invalid#synthetic", "scalar golden");

    // Expected authority components are literal fixtures; only query/fragment
    // serialization uses an independent Qt URL. No range importer is present.
    const QList<std::pair<QString, QString>> hosts = {
        {"example.invalid", "example.invalid"}, {"192.0.2.1", "192.0.2.1"},
        {"2001:db8::1", "[2001:db8::1]"}, {"2001:db8:0:0:1:0:0:1", "[2001:db8::1:0:0:1]"}
    };
    const QList<std::pair<QString, QString>> credentials = {
        {"synthetic-auth", "synthetic-auth"}, {"synthetic-user:synthetic-pass", "synthetic-user:synthetic-pass"},
        {"user@name:p@ss?word#tail", "user%40name:p%40ss%3Fword%23tail"},
        {"user:0:0?tail", "user:0:0%3Ftail"}, {"literal%3A0", "literal%253A0"}
    };
    const QList<std::pair<QStringList, QString>> ranges = {
        {{"443:445"}, "443-445"}, {{"443", "8443"}, "443,8443"},
        {{"443:445", "8443", "9000:9002"}, "443-445,8443,9000-9002"},
        {{"0:1", "65535"}, "0-1,65535"}, {{"443:443"}, "443-443"},
        {{"443"}, "443"}
    };
    const QStringList names = {"", "synthetic", "marker:0?tail:0#next", "percent%3A0 space 日本語"};
    for (const auto type : {QUICBean::proxy_Hysteria, QUICBean::proxy_Hysteria2}) {
        for (const auto &[host, encodedHost] : hosts) {
            for (const auto &[auth, encodedAuth] : credentials) {
                for (const auto &name : names) {
                    for (int queryMode = 0; queryMode < 6; ++queryMode) {
                        ProxyEntity ent;
                        ent.serverAddress = host;
                        ent.serverPort = 8443;
                        ent.name = name;
                        QUICBean value(&ent, type);
                        value.password = auth;
                        QString prefix = type == QUICBean::proxy_Hysteria ? "hysteria://" : "hy2://" + encodedAuth + "@";
                        prefix += encodedHost + ":";
                        QUrl suffixUrl;
                        QUrlQuery expectedQuery;
                        if (type == QUICBean::proxy_Hysteria) {
                            expectedQuery.addQueryItem("upmbps", "100");
                            expectedQuery.addQueryItem("downmbps", "100");
                        }
                        if (queryMode == 1) {
                            value.sni = "sni.invalid";
                            expectedQuery.addQueryItem(type == QUICBean::proxy_Hysteria ? "peer" : "sni", "sni.invalid");
                        } else if (queryMode == 2) {
                            value.sni = "marker:0?tail:0";
                            expectedQuery.addQueryItem(type == QUICBean::proxy_Hysteria ? "peer" : "sni", "marker:0?tail:0");
                        } else if (queryMode == 3) {
                            value.allowInsecure = true;
                            expectedQuery.addQueryItem("insecure", "1");
                        } else if (queryMode == 4) {
                            value.hop_interval = "10s";
                            expectedQuery.addQueryItem("hop_interval", "10s");
                        } else if (queryMode == 5) {
                            *value.network = "udp";
                            value.network->value = 1;
                            expectedQuery.addQueryItem("network", "udp");
                        }
                        if (!expectedQuery.isEmpty()) suffixUrl.setQuery(expectedQuery);
                        if (!name.isEmpty()) suffixUrl.setFragment(name);
                        const auto suffix = suffixUrl.toString(QUrl::FullyEncoded);
                        for (const auto &[input, ports] : ranges) {
                            value.serverPorts = input;
                            const QString label = "type=" + QString::number(type) + " host=" + host +
                                " auth=" + auth + " name=" + name + " query=" + QString::number(queryMode) + " ports=" + ports;
                            equal(value.ToShareLink(), prefix + ports + suffix, label);
                            equal(value.serverPorts.join(','), input.join(','), "export leaves bean ranges unchanged");
                            equal(ent.name, name, "export leaves name unchanged");
                        }
                        value.serverPorts.clear();
                        for (int port : {0, 1, 443, 8443, 65535}) {
                            ent.serverPort = port;
                            equal(value.ToShareLink(), prefix + QString::number(port) + suffix, "scalar exact bytes control");
                        }
                    }
                }
            }
        }
    }
    // Boundary fixtures from independent native-Qt review. These assert the
    // exact encoded authority length, not decoded character counts.
    const QList<std::pair<QString, QString>> extraHosts = {
        {"éxample.invalid", "xn--xample-9ua.invalid"},
        {"日本語.invalid", "xn--wgv71a119e.invalid"},
        {"fe80::1%eth0", "[fe80::1%25eth0]"}
    };
    entity.name.clear();
    bean.sni.clear();
    bean.serverPorts = {"443:445"};
    bean.password = "日本語:⚡";
    for (const auto &[host, encoded] : extraHosts) {
        entity.serverAddress = host;
        equal(bean.ToShareLink(), "hy2://%E6%97%A5%E6%9C%AC%E8%AA%9E:%E2%9A%A1@" + encoded + ":443-445", "Unicode/scoped authority");
    }
    entity.serverAddress = "example.invalid";
    bean.password.clear();
    equal(bean.ToShareLink(), "hy2://example.invalid:443-445", "null auth authority");
    bean.password = "";
    equal(bean.ToShareLink(), "hy2://@example.invalid:443-445", "non-null empty auth authority");
    bean.password = "synthetic-auth";
    bean.sni = "sni.invalid";
    bean.serverPorts = {"443:400", "abc:def", "65536"};
    equal(bean.ToShareLink(), "hy2://synthetic-auth@example.invalid:443-400,abc-def,65536?sni=sni.invalid", "malformed stored ranges remain unvalidated");

    // QUrl's existing setter sequence clears malformed hosts before export;
    // preserve the existing empty-host output and only repair its range slot.
    for (const QString host : {"", "bad host", "[2001:db8::xyz]", "[", "example.invalid/evil", "2001:db8::xyz"}) {
        for (const auto type : {QUICBean::proxy_Hysteria, QUICBean::proxy_Hysteria2}) {
            ProxyEntity invalidEntity;
            invalidEntity.serverAddress = host;
            QUICBean invalid(&invalidEntity, type);
            invalid.password = "synthetic-auth";
            invalid.serverPorts = {"443:445"};
            const QString expected = type == QUICBean::proxy_Hysteria
                ? "hysteria://:443-445?upmbps=100&downmbps=100"
                : "hy2://synthetic-auth@:443-445";
            equal(invalid.ToShareLink(), expected, "existing empty/malformed host behavior");
            invalid.sni = "sni.invalid";
            equal(invalid.ToShareLink(), expected + (type == QUICBean::proxy_Hysteria ? "&peer=sni.invalid" : "?sni=sni.invalid"), "malformed host query control");
        }
    }

    // TUIC does not participate in port-range export. Both its ordinary port
    // and marker-like credential/fragment text must keep their exact bytes.
    ProxyEntity tuicEntity;
    tuicEntity.serverAddress = "2001:db8::1";
    tuicEntity.serverPort = 8443;
    tuicEntity.name = "marker:0?tail";
    QUICBean tuic(&tuicEntity, QUICBean::proxy_TUIC);
    tuic.uuid = "synthetic-user";
    tuic.password = "marker:0?tail";
    tuic.serverPorts = {"443:445"};
    equal(tuic.ToShareLink(), "tuic://synthetic-user:marker:0%3Ftail@[2001:db8::1]:8443?congestion_control=bbr&udp_relay_mode=native#marker:0?tail", "TUIC control");
    std::cout << "RESULT " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
