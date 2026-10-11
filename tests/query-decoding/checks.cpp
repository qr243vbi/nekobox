int total = 0, failed = 0;
void check(bool pass, const QString &label) {
    ++total;
    if (!pass) { ++failed; std::cout << "FAIL " << label.toStdString() << '\n'; }
}
void same(const QString &got, const QString &expected, const QString &label) {
    check(got == expected, label);
    if (got != expected) std::cout << "  expected=" << expected.toUtf8().toHex().constData()
                                  << " actual=" << got.toUtf8().toHex().constData() << '\n';
}
QString encoded(const QString &value) { return QString::fromLatin1(QUrl::toPercentEncoding(value)); }
QString uri(const QString &scheme, const QString &query) {
    return scheme + "://fixture:password@example.invalid:443?" + query + "#fixture";
}
Configs::ProxyEntity entity() { return {"fixture", "example.invalid", "", 443}; }
const QStringList payloads = {
    "plain", "a&b=c#d%", "a&allowInsecure=1&insecure=1&uot=2", "AA+/==",
    "literal%26allowInsecure%3D1", ">>>????", "literal%2526", "literal%25", "%", "%2", "%zz",
    "-----BEGIN ECH CONFIGS-----\nAA+/==\n-----END ECH CONFIGS-----\n",
    "-----BEGIN ECH CONFIGS-----\r\nAA+/==\r\n-----END ECH CONFIGS-----\r\n",
    QString::fromUtf8("é雪 +/%26")
};

template<typename T> void sharedTLS(const QString &scheme) {
    for (const auto &value : payloads) {
        auto e = entity(); T b(&e);
        check(b.TryParseLink(uri(scheme, "ech_config=" + encoded(value) + "&fragment_fallback_delay=" + encoded(value) + "&query_server_name=query.invalid")), scheme + " import");
        same(b.stream->ech_config, value, scheme + " opaque ECH");
        same(b.stream->tls_fragment_fallback_delay, value, scheme + " fragment delay");
        check(!b.stream->allow_insecure, scheme + " prevents insecure injection");
        auto e2 = entity(); T copy(&e2);
        b.stream->security = "tls";
        b.stream->utlsFingerprint = value;
        check(copy.TryParseLink(b.ToShareLink()), scheme + " roundtrip import");
        same(copy.stream->ech_config, value, scheme + " ECH roundtrip");
        same(copy.stream->utlsFingerprint, value, scheme + " fingerprint roundtrip");
        check(!copy.stream->allow_insecure, scheme + " roundtrip remains secure");
    }
    auto e = entity(); T positive(&e);
    check(positive.TryParseLink(uri(scheme, "insecure=1")) && positive.stream->allow_insecure, scheme + " explicit insecure works");
}

int main() {
    using namespace Configs;
    for (const auto &value : payloads) {
        const QUrl url(uri("trojan", "opaque=" + encoded(value) + "&real=kept"));
        auto query = GetQuery(url);
        same(GetQueryValue(query, "opaque"), value, "query single decode");
        same(GetQueryValue(query, "real"), "kept", "real boundary preserved");
        check(query.queryItems().size() == 2, "encoded delimiters never create items");
        check(!query.hasQueryItem("allowInsecure") && !query.hasQueryItem("insecure"), "no injected flags");
        auto stream = std::make_shared<V2rayStreamSettings>();
        stream->security = value;
        const QString saved = dataStore->utlsFingerprint;
        dataStore->utlsFingerprint = value;
        QUrlQuery tlsQuery; Configs::To_Link::add_tls(stream, tlsQuery);
        same(GetQueryValue(tlsQuery, "fp"), value, "default fingerprint export preserves text");
        same(GetQueryValue(tlsQuery, "security"), value, "security selector export preserves text");
        dataStore->utlsFingerprint = saved;
        QUrl exported("https://example.invalid"); QUrlQuery q;
        AddQueryString(q, "opaque", value); exported.setQuery(q);
        const QUrl reread(exported.toString(QUrl::FullyEncoded)); auto parsed = GetQuery(reread);
        same(GetQueryValue(parsed, "opaque"), value, "query writer percent-safe roundtrip");
    }
    for (const QString &text : {QString("AA+/=="), QString("AA%2B%2F%3D%3D")}) {
        QUrl url(uri("anytls", "ech_config=" + text)); auto query = GetQuery(url);
        same(GetQueryValue(query, "ech_config"), "AA+/==", "raw and escaped Base64 plus/slash/padding");
        auto e = entity(); AnyTLSBean bean(&e);
        check(bean.TryParseLink(uri("anytls", "ech_config=" + text)), "Base64 shared TLS parser");
        same(bean.stream->ech_config, "AA+/==", "Base64 shared TLS content");
    }
    QUrl emptyURL(uri("trojan", "empty=&n=12&duplicate=first&duplicate=second")); auto empty = GetQuery(emptyURL);
    same(GetQueryValue(empty, "empty", "fallback"), "fallback", "empty fallback unchanged");
    same(GetQueryValue(empty, "missing", "fallback"), "fallback", "missing fallback unchanged");
    same(GetQueryValue(empty, "duplicate"), "first", "duplicate precedence unchanged");
    check(GetQueryIntValue(empty, "n") == 12 && GetQueryIntValue(empty, "missing", 9) == 9, "integer getters unchanged");

    for (int kind : {TrojanVLESSBean::proxy_Trojan, TrojanVLESSBean::proxy_VLESS}) {
        QString scheme = kind == TrojanVLESSBean::proxy_Trojan ? "trojan" : "vless";
        for (const auto &value : payloads) {
            auto e = entity(); TrojanVLESSBean b(&e, kind);
            check(b.TryParseLink(uri(scheme, "security=tls&type=ws&path=" + encoded(value) + "&fragment_fallback_delay=" + encoded(value))), scheme + " parser");
            same(b.stream->path, value, scheme + " opaque path");
            same(b.stream->tls_fragment_fallback_delay, value, scheme + " fragment delay");
            check(!b.stream->allow_insecure, scheme + " prevents allowInsecure injection");
            if (kind == TrojanVLESSBean::proxy_VLESS) b.encryption = value;
            auto e2 = entity(); TrojanVLESSBean copy(&e2, kind);
            check(copy.TryParseLink(b.ToShareLink()), scheme + " roundtrip parser");
            same(copy.stream->path, value, scheme + " path roundtrip");
            if (kind == TrojanVLESSBean::proxy_VLESS) same(copy.encryption, value, "VLESS encryption roundtrip");
            check(!copy.stream->allow_insecure, scheme + " roundtrip secure");
        }
        auto e = entity(); TrojanVLESSBean b(&e, kind);
        check(b.TryParseLink(uri(scheme, "allowInsecure=1")) && b.stream->allow_insecure, scheme + " explicit allowInsecure preserved");
    }
    sharedTLS<AnyTLSBean>("anytls"); sharedTLS<HttpBean>("https"); sharedTLS<ShadowTLSBean>("shadowtls");
    sharedTLS<NaiveBean>("naive+https"); sharedTLS<JuicityBean>("juicity"); sharedTLS<TrustTunnelBean>("trusttunnel");

    for (const auto &value : payloads) {
        auto e = entity(); VMessBean b(&e);
        check(b.TryParseLink(uri("vmess", "type=ws&path=" + encoded(value) + "&fragment_fallback_delay=" + encoded(value))), "VMess URI parser");
        same(b.stream->path, value, "VMess opaque path");
        same(b.stream->tls_fragment_fallback_delay, value, "VMess fragment delay");
        check(!b.stream->allow_insecure, "VMess no insecure injection");
        auto e2 = entity(); VMessBean copy(&e2);
        check(copy.TryParseLink(b.ToShareLink()), "VMess URI roundtrip parser");
        same(copy.stream->path, value, "VMess path roundtrip");

        SSHBean ssh(&e);
        QString query;
        for (auto key : {"user", "password", "private_key_path", "private_key_passphrase", "client_version"})
            query += QString(key) + '=' + encoded(value) + '&';
        query += "private_key=" + encoded(value.toUtf8().toBase64()) + "&host_key=" + encoded(value.toUtf8().toBase64()) + "&host_key_algorithms=" + encoded(value.toUtf8().toBase64());
        check(ssh.TryParseLink(uri("ssh", query)), "SSH parser");
        same(ssh.user, value, "SSH user"); same(ssh.password, value, "SSH password");
        same(ssh.privateKeyPath, value, "SSH path"); same(ssh.privateKeyPass, value, "SSH passphrase");
        same(ssh.hostKey.value(0), value, "SSH host key Base64"); same(ssh.hostKeyAlgs.value(0), value, "SSH algorithms Base64");
        same(ssh.clientVersion, value, "SSH client version"); same(ssh.privateKey, value, "SSH Base64 key");
        SSHBean sshCopy(&e2); check(sshCopy.TryParseLink(ssh.ToShareLink()), "SSH roundtrip parser");
        same(sshCopy.password, value, "SSH password roundtrip"); same(sshCopy.privateKey, value, "SSH key roundtrip");

        TailscaleBean ts(&e);
        query.clear();
        for (auto key : {"state_directory", "auth_key", "control_url", "hostname", "exit_node", "advertise_routes"})
            query += QString(key) + '=' + encoded(value) + '&';
        check(ts.TryParseLink(uri("ts", query)), "Tailscale parser");
        same(ts.state_directory, value, "Tailscale directory"); same(ts.auth_key, value, "Tailscale auth key");
        same(ts.control_url, value, "Tailscale control URL"); same(ts.hostname, value, "Tailscale hostname");
        same(ts.exit_node, value, "Tailscale exit node"); same(ts.advertise_routes.join(","), value, "Tailscale advertised routes");
        TailscaleBean tsCopy(&e2); check(tsCopy.TryParseLink(ts.ToShareLink()), "Tailscale roundtrip parser");
        same(tsCopy.state_directory, value, "Tailscale directory roundtrip"); same(tsCopy.control_url, value, "Tailscale control roundtrip");
        same(tsCopy.hostname, value, "Tailscale hostname roundtrip"); same(tsCopy.advertise_routes.join(","), value, "Tailscale routes roundtrip");
        same(tsCopy.auth_key, value, "Tailscale auth roundtrip"); same(tsCopy.exit_node, value, "Tailscale exit roundtrip");

        ShadowSocksBean ss(&e);
        check(ss.TryParseLink(uri("ss", "plugin=" + encoded(value))), "Shadowsocks URI parser");
        same(ss.plugin, value, "Shadowsocks opaque plugin"); check(ss.uot == 0, "Shadowsocks no injected uot");
        ShadowSocksBean ssCopy(&e2); check(ssCopy.TryParseLink(ss.ToShareLink()), "Shadowsocks roundtrip parser");
        same(ssCopy.plugin, value, "Shadowsocks plugin roundtrip");

        WireguardBean wg(&e);
        check(wg.TryParseLink(uri("wg", "private_key=" + encoded(value) + "&peer_public_key=" + encoded(value))), "WireGuard URI parser");
        same(wg.privateKey, value, "WireGuard private key"); same(wg.publicKey, value, "WireGuard peer key");
        WireguardBean wgCopy(&e2); check(wgCopy.TryParseLink(wg.ToShareLink()), "WireGuard URI roundtrip parser");
        same(wgCopy.privateKey, value, "WireGuard key roundtrip");

        MieruBean mieru(&e);
        check(mieru.TryParseLink(uri("mierus", "traffic_pattern=" + encoded(value) + "&multiplexing=" + encoded(value))), "Mieru parser");
        same(mieru.traffic_pattern, value, "Mieru opaque traffic pattern");
        same(*mieru.multiplexing, value, "Mieru multiplexing single decode");
        MieruBean mieruCopy(&e2); check(mieruCopy.TryParseLink(mieru.ToShareLink()), "Mieru roundtrip parser");
        same(mieruCopy.traffic_pattern, value, "Mieru traffic pattern roundtrip");

        QString map = QString::fromUtf8(QJsonDocument(QJsonObject{{"X-Fixture", value}}).toJson(QJsonDocument::Compact));
        QString list = QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact));
        TorBean tor(&e);
        check(tor.TryParseLink(uri("tor", "executable_path=" + encoded(value) + "&torrc=" + encoded(map) + "&extra_args=" + encoded(list))), "Tor parser");
        same(tor.executable_path, value, "Tor opaque path"); same(tor.torrc.value("X-Fixture").toString(), value, "Tor nested JSON map");
        same(tor.extra_args.value(0), value, "Tor nested JSON list");
        TorBean torCopy(&e2); check(torCopy.TryParseLink(tor.ToShareLink()), "Tor roundtrip parser");
        same(torCopy.torrc.value("X-Fixture").toString(), value, "Tor nested map roundtrip");

        HttpBean http(&e); check(http.TryParseLink(uri("http", "headers=" + encoded(map))), "HTTP headers parser");
        same(http.headers.value("X-Fixture").toString(), value, "HTTP nested JSON headers");
        NaiveBean naive(&e); check(naive.TryParseLink(uri("naive+https", "extra_headers=" + encoded(map))), "Naive headers parser");
        same(naive.extra_headers.value("X-Fixture").toString(), value, "Naive nested JSON headers");

        SocksBean socks(&e, SocksBean::type_Socks5);
        check(socks.TryParseLink(uri("socks5", "unknown=" + encoded(value))), "SOCKS parser");
        check(socks.uot == 0, "SOCKS no injected uot");
    }
    for (int kind : {QUICBean::proxy_Hysteria, QUICBean::proxy_Hysteria2, QUICBean::proxy_TUIC}) {
        const QString scheme = kind == QUICBean::proxy_Hysteria ? "hysteria" : kind == QUICBean::proxy_Hysteria2 ? "hy2" : "tuic";
        for (const auto &value : payloads) {
            auto qe = entity(); QUICBean quic(&qe, kind);
            QString query = "upmbps=10&downmbps=20&sni=" + encoded(value) + "&alpn=" + encoded(value)
                + "&auth=" + encoded(value) + "&obfsParam=" + encoded(value) + "&obfs-password=" + encoded(value);
            check(quic.TryParseLink(uri(scheme, query)), scheme + " parser");
            same(quic.sni, value, scheme + " SNI single decode");
            if (kind != QUICBean::proxy_Hysteria2) same(quic.alpn, value, scheme + " ALPN single decode");
            if (kind != QUICBean::proxy_TUIC) same(quic.obfsPassword, value, scheme + " obfs single decode");
            if (kind == QUICBean::proxy_Hysteria) same(quic.authPayload, value, "Hysteria auth single decode");
            check(!quic.allowInsecure, scheme + " no insecure injection");
            auto qe2 = entity(); QUICBean copy(&qe2, kind);
            check(copy.TryParseLink(quic.ToShareLink()), scheme + " roundtrip parser");
            same(copy.sni, value, scheme + " SNI roundtrip");
            if (kind != QUICBean::proxy_Hysteria2) same(copy.alpn, value, scheme + " ALPN roundtrip");
            if (kind != QUICBean::proxy_TUIC) same(copy.obfsPassword, value, scheme + " obfs roundtrip");
            if (kind == QUICBean::proxy_Hysteria) same(copy.authPayload, value, "Hysteria auth roundtrip");
            check(!copy.allowInsecure, scheme + " roundtrip secure");
        }
    }
    auto controlEntity = entity();
    QUICBean emptyAuth(&controlEntity, QUICBean::proxy_Hysteria);
    check(emptyAuth.TryParseLink(uri("hysteria", "upmbps=10&downmbps=20&auth=")), "Hysteria explicit empty auth import");
    check(emptyAuth.authPayloadType == QUICBean::hysteria_auth_string && emptyAuth.authPayload.isEmpty(), "Hysteria explicit empty string mode");
    const auto emptyAuthLink = emptyAuth.ToShareLink();
    check(QUrlQuery(QUrl(emptyAuthLink)).hasQueryItem("auth"), "Hysteria empty auth item retained on export");
    QUICBean emptyAuthCopy(&controlEntity, QUICBean::proxy_Hysteria);
    check(emptyAuthCopy.TryParseLink(emptyAuthLink), "Hysteria empty auth roundtrip parser");
    check(emptyAuthCopy.authPayloadType == QUICBean::hysteria_auth_string && emptyAuthCopy.authPayload.isEmpty(), "Hysteria empty string mode preserved");
    MieruBean transport(&controlEntity);
    check(transport.TryParseLink(uri("mierus", "transport=T%43P&server_ports=443%2C8443")), "Mieru escaped controls parser");
    same(*transport.network, "tcp", "Mieru case normalization preserved");
    check(transport.serverPorts == QStringList{"443", "8443"}, "Mieru server ports preserved");
    WireguardBean controls(&controlEntity);
    check(controls.TryParseLink(uri("wg", "reserved=1%2D2%2D3&local_address=192.0.2.1%2F32&pre_shared_key=AA%2B%2F%3D%3D")), "WireGuard escaped controls parser");
    check(controls.reserved == QList<int>{1,2,3}, "WireGuard reserved bytes preserved");
    same(controls.localAddress.value(0), "192.0.2.1/32", "WireGuard address preserved");
    same(controls.preSharedKey, "AA+/==", "WireGuard preshared Base64 preserved");
    for (const QString &scheme : {QString("socks4"), QString("socks5")}) {
        SocksBean socks(&controlEntity, SocksBean::type_Socks5);
        check(socks.TryParseLink(uri(scheme, "uot=2")), scheme + " supported URI");
        check(socks.uot == 2, scheme + " explicit uot preserved");
    }
    QUICBean alias(&controlEntity, QUICBean::proxy_Hysteria2);
    check(alias.TryParseLink(uri("hysteria2", "obfs-password=AA%2B%2F%3D%3D&insecure=1")) && alias.allowInsecure, "Hysteria2 alias and explicit insecure");
    same(alias.obfsPassword, "AA+/==", "Hysteria2 alias obfs Base64");
    NaiveBean naiveAlias(&controlEntity);
    check(naiveAlias.TryParseLink(uri("naive+quic", "quic=true")) && naiveAlias.quic, "Naive QUIC alias");
    ShadowSocksBean ss2022(&controlEntity);
    check(ss2022.TryParseLink("ss://2022-blake3-aes-128-gcm:fixture@example.invalid:443?plugin=obfs-local%3Bobfs%3Dhttp"), "Shadowsocks 2022 form");
    same(ss2022.plugin, "obfs-local;obfs=http", "Shadowsocks 2022 plugin");
    // Preserve supported legacy forms; query changes must not affect these paths.
    auto e = entity(); ShadowSocksBean legacySS(&e);
    QString ssBody = "aes-128-gcm:fixture-password@example.invalid:443";
    check(legacySS.TryParseLink("ss://" + ssBody.toUtf8().toBase64(QByteArray::Base64UrlEncoding) + "#legacy"), "Shadowsocks legacy Base64 form");
    same(legacySS.password, "fixture-password", "Shadowsocks legacy password");
    VMessBean legacyVM(&e);
    QString vmBody = QString::fromUtf8(QJsonDocument(QJsonObject{{"id", "fixture"}, {"add", "example.invalid"}, {"port", 443}, {"path", "/literal%26&value"}}).toJson(QJsonDocument::Compact));
    check(legacyVM.TryParseLink("vmess://" + vmBody.toUtf8().toBase64()), "VMess legacy Base64 JSON form");
    same(legacyVM.stream->path, "/literal%26&value", "VMess JSON text unaffected");
    // HTTP's scheme determines both the implicit port and TLS. Exercise real
    // QUrl normalization, then reimport the production export (issue #68).
    for (const QString scheme : {"http", "HTTP", "HtTp", "https", "HTTPS", "HtTpS"}) {
        const bool tls = scheme.compare("https", Qt::CaseInsensitive) == 0;
        for (const QString authority : {"example.invalid", "[2001:db8::1]"}) {
            for (int port : {-1, 80, 443, 8080}) {
                for (bool insecure : {false, true}) {
                    const QString label = scheme + " " + authority + " port=" + QString::number(port)
                        + " insecure=" + QString::number(insecure);
                    const QString input = scheme + "://fixture:secret@" + authority
                        + (port == -1 ? "" : ":" + QString::number(port))
                        + "/proxy/path?insecure=" + (insecure ? "1" : "0")
                        + "&sni=sni.invalid&headers=" + encoded("{\"X-Fixture\":\"literal%26&value\"}")
                        + "#HTTP%20fixture";
                    auto httpEntity = entity(); HttpBean bean(&httpEntity);
                    check(bean.TryParseLink(input), label + " parses");
                    const int expectedPort = port == -1 ? (tls ? 443 : 80) : port;
                    check(httpEntity.serverPort == expectedPort, label + " scheme default/explicit port");
                    same(httpEntity.serverAddress, QUrl(input).host(), label + " destination");
                    same(bean.stream->security, tls ? "tls" : "", label + " TLS follows scheme");
                    check(bean.stream->allow_insecure == (tls && insecure), label + " TLS certificate option");
                    same(bean.username, "fixture", label + " username");
                    same(bean.password, "secret", label + " password");
                    same(bean.path, "/proxy/path", label + " path");
                    same(bean.headers.value("X-Fixture").toString(), "literal%26&value", label + " headers");
                    same(httpEntity.name, "HTTP fixture", label + " name");
                    const QString exported = bean.ToShareLink();
                    same(QUrl(exported).scheme(), tls ? "https" : "http", label + " export scheme");
                    auto copyEntity = entity(); HttpBean copy(&copyEntity);
                    check(copy.TryParseLink(exported), label + " reimports");
                    check(copyEntity.serverPort == expectedPort, label + " roundtrip port");
                    same(copyEntity.serverAddress, httpEntity.serverAddress, label + " roundtrip destination");
                    same(copy.stream->security, bean.stream->security, label + " roundtrip TLS");
                    check(copy.stream->allow_insecure == bean.stream->allow_insecure, label + " roundtrip certificate option");
                    if (tls) same(copy.stream->sni, "sni.invalid", label + " roundtrip SNI");
                    same(copy.username, bean.username, label + " roundtrip username");
                    same(copy.password, bean.password, label + " roundtrip password");
                    same(copy.path, bean.path, label + " roundtrip path");
                    check(copy.headers == bean.headers, label + " roundtrip headers");
                }
            }
        }
    }
    for (const QString input : {"ftp://example.invalid", "https-proxy://example.invalid",
                               "example.invalid", "https:///", "https://example.invalid:invalid"}) {
        auto httpEntity = entity(); HttpBean bean(&httpEntity);
        check(!bean.TryParseLink(input), input + " HTTP parser rejects invalid/unsupported URI");
    }
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
