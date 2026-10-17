
// ---- import compatibility checks ---------------------------------------
// The same inputs and expectations are run against this branch and against a
// revision without chain import (--ref), through RawUpdater::update().

static int checks = 0, failures = 0;
#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        ++checks;                                                              \
        const auto va = (a);                                                   \
        const auto vb = (b);                                                   \
        if (!(va == vb)) {                                                     \
            ++failures;                                                        \
            std::cout << "FAIL line " << __LINE__ << ": " #a " == " #b         \
                      << "\n  got:      " << toText(va).toStdString()          \
                      << "\n  expected: " << toText(vb).toStdString() << std::endl; \
        }                                                                      \
    } while (0)

static QString toText(const QString &s) { return s; }
static QString toText(int v) { return QString::number(v); }
static QString toText(bool v) { return v ? "true" : "false"; }
static QString toText(const QStringList &l) { return "[" + l.join(" | ") + "]"; }

using Subscription::RawUpdater;

// What GroupUpdater::Update() does with the text of a paste or subscription.
static QStringList importText(const QString &text) {
    auto &pm = *Configs::profileManager;
    pm.ents.clear();
    pm.groups.clear();
    pm.maxId = 0;
    pm.groups[1] = std::make_shared<Configs::Group>();
    logLines.clear();
    RawUpdater u;
    u.update(text);
    pm.AddProfileBatch(u.proxies, u.gid_add_to);
#ifdef HAS_CHAINS
    u.FinalizeChains();
#endif
    QStringList out;
    for (int id : pm.GetGroup(1)->profiles) {
        auto e = pm.GetProfile(id);
        QString row = e->type + " " + e->name;
        if (e->type == "custom") row += " [" + e->CustomBean()->core + "]";
        out << row;
    }
    return out;
}

static QString base64(const QString &text) { return QString::fromLatin1(text.toUtf8().toBase64()); }
static Configs::ProxyEntity *only() {
    auto &pm = *Configs::profileManager;
    return pm.GetGroup(1)->profiles.size() == 1 ? pm.GetProfile(pm.GetGroup(1)->profiles.first()).get() : nullptr;
}

// The full config a server used to hand out: dns and route make it internal-full.
static const char *kFullConfig = R"j({"remarks":"client (SSH → VLESS)","log":{"level":"warn"},
 "dns":{"servers":[{"type":"https","tag":"dns-remote","server":"1.1.1.1","detour":"proxy"}],"final":"dns-remote"},
 "outbounds":[
  {"type":"vless","tag":"proxy","server":"127.0.0.1","server_port":10085,"uuid":"u","packet_encoding":"xudp","detour":"ssh-hop"},
  {"type":"ssh","tag":"ssh-hop","server":"h.invalid","server_port":4422,"user":"tunnel","private_key":"KEY\nLINE\n","host_key":["ssh-ed25519 AAAA"]},
  {"type":"direct","tag":"direct"}],
 "route":{"final":"proxy","auto_detect_interface":true}})j";

static const char *kPlainOutbounds = R"j({"remarks":"r","outbounds":[
  {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u"},
  {"type":"ssh","tag":"b","server":"b.invalid","server_port":22,"user":"x"},
  {"type":"direct","tag":"direct"}]})j";

static const char *kLinks = "vless://u@a.invalid:443?security=tls#first\r\n"
                            "\r\n"
                            "vless://u@b.invalid:8443#second\n";

static void testFullConfigStaysInternalFull() {
    for (const auto &text : {QString(kFullConfig), base64(kFullConfig)}) {
        CHECK_EQ(importText(text), (QStringList{"custom client (SSH → VLESS) [internal-full]"}));
        auto e = only();
        if (e == nullptr) continue;
        CHECK_EQ(e->display_type, QString("VLESS/JSON"));
        auto config = QJsonDocument::fromJson(e->CustomBean()->config_simple.toUtf8()).object();
        auto outbounds = config["outbounds"].toArray();
        CHECK_EQ(int(outbounds.size()), 3);
        CHECK_EQ(outbounds[0].toObject()["detour"].toString(), QString("ssh-hop"));
        CHECK_EQ(outbounds[1].toObject()["tag"].toString(), QString("ssh-hop"));
        CHECK_EQ(config["route"].toObject()["final"].toString(), QString("proxy"));
        CHECK_EQ(int(config["inbounds"].toArray().size()), 0);
    }
}

static void testOutboundsWithoutDetourStaySeparate() {
    for (const auto &text : {QString(kPlainOutbounds), base64(kPlainOutbounds)}) {
        CHECK_EQ(importText(text), (QStringList{"vless a", "ssh b"}));
    }
}

static void testLinksAndBase64Subscription() {
    CHECK_EQ(importText(kLinks), (QStringList{"vless first", "vless second"}));
    CHECK_EQ(importText(base64(kLinks)), (QStringList{"vless first", "vless second"}));
    // nested base64, as some panels serve it
    CHECK_EQ(importText(base64(base64(kLinks))), (QStringList{"vless first", "vless second"}));
}

static void testJsonArraySubscription() {
    const QString text = R"j([
      {"remarks":"one","outbounds":[{"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u"}]},
      "vless://u@c.invalid:443#third"])j";
    CHECK_EQ(importText(text), (QStringList{"vless one", "vless third"}));
}

static void testSingleOutboundAndSip008() {
    const QString single = R"j({"type":"vless","tag":"x","server":"x.invalid","server_port":443,"uuid":"u"})j";
    CHECK_EQ(importText(single), (QStringList{"custom  [internal]"}));
    if (auto e = only()) CHECK_EQ(e->CustomBean()->config_simple, single);
    const QString sip = R"j({"version":1,"servers":[
      {"server":"s.invalid","server_port":8388,"password":"p","method":"aes-256-gcm","remarks":"ss"}]})j";
    CHECK_EQ(importText(sip), (QStringList{"shadowsocks ss"}));
}

#ifdef HAS_CHAINS
// Not a compatibility case: the new format through the same dispatch.
static void testChainThroughDispatch() {
    const QString text = R"j({"remarks":"client","outbounds":[
      {"type":"vless","tag":"inner","server":"127.0.0.1","server_port":10085,"uuid":"u","detour":"ssh-hop"},
      {"type":"ssh","tag":"ssh-hop","server":"h.invalid","server_port":4422,"user":"tunnel"},
      {"type":"vless","tag":"reality","server":"h.invalid","server_port":443,"uuid":"u"}]})j";
    const QStringList want{"chain client", "vless client / reality", "ssh client / ssh", "vless client / vless"};
    CHECK_EQ(importText(text), want);
    CHECK_EQ(importText(base64(text)), want);
    CHECK_EQ(importText("[" + text + "]"), want);
}
#endif

int main() {
    testFullConfigStaysInternalFull();
    testOutboundsWithoutDetourStaySeparate();
    testLinksAndBase64Subscription();
    testJsonArraySubscription();
    testSingleOutboundAndSip008();
#ifdef HAS_CHAINS
    testChainThroughDispatch();
    std::cout << "MODE: with chain import" << std::endl;
#else
    std::cout << "MODE: without chain import" << std::endl;
#endif
    std::cout << "RESULT " << (checks - failures) << "/" << checks << " checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
