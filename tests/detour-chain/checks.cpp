
// ---- checks -------------------------------------------------------------

static int checks = 0, failures = 0;
#define CHECK(cond)                                                            \
    do {                                                                       \
        ++checks;                                                              \
        if (!(cond)) {                                                         \
            ++failures;                                                        \
            std::cout << "FAIL line " << __LINE__ << ": " #cond << std::endl; \
        }                                                                      \
    } while (0)
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
static QString toText(qsizetype v) { return QString::number(v); }
static QString toText(bool v) { return v ? "true" : "false"; }
static QString toText(const QList<int> &l) {
    QStringList p;
    for (int i : l) p << QString::number(i);
    return "[" + p.join(",") + "]";
}
static QString toText(const QStringList &l) { return "[" + l.join(" | ") + "]"; }

using Subscription::RawUpdater;
using Ent = std::shared_ptr<Configs::ProxyEntity>;

static void resetWorld() {
    auto &pm = *Configs::profileManager;
    pm.ents.clear();
    pm.groups.clear();
    pm.maxId = 0;
    pm.lockDepth = pm.maxLockDepth = 0;
    pm.groups[1] = std::make_shared<Configs::Group>();
    pm.groups[2] = std::make_shared<Configs::Group>();
    pm.groups[2]->id = 2;
    logLines.clear();
}

static QJsonObject parse(const char *text) {
    QJsonParseError err{};
    auto doc = QJsonDocument::fromJson(QByteArray(text), &err);
    if (err.error != QJsonParseError::NoError) {
        std::cout << "BAD FIXTURE: " << err.errorString().toStdString() << "\n" << text << std::endl;
        std::exit(2);
    }
    return doc.object();
}

// number of chain groups the last parse found, before FinalizeChains() consumed them
static int lastUnits = 0;

// What GroupUpdater::Update() does with one input string.
static void importString(RawUpdater &u, const char *text, bool asArrayElement = false) {
    auto obj = parse(text);
    u.updateSingBox(obj, asArrayElement ? obj["remarks"].toString() : QString());
    Configs::profileManager->AddProfileBatch(u.proxies, u.gid_add_to);
    lastUnits = u.chain_imports.size();
    u.FinalizeChains();
}

static Ent ent(int id) { return Configs::profileManager->GetProfile(id); }
static QList<int> groupOf(int gid = 1) { return Configs::profileManager->GetGroup(gid)->profiles; }
static QStringList names(int gid = 1) {
    QStringList out;
    for (int id : groupOf(gid)) out << ent(id)->name;
    return out;
}
static QList<int> chainList(int id) { return ent(id)->ChainBean()->list; }
static bool logged(const QString &part) {
    for (const auto &l : logLines)
        if (l.contains(part)) return true;
    return false;
}

// ---- fixtures -----------------------------------------------------------

// The string a server hands out: a chain (inner VLESS over SSH) and a
// standalone Reality profile. Values are placeholders.
static const char *kChainAndReality = R"({
  "remarks": "client", "log": {"level": "warn"},
  "outbounds": [
    {"type":"vless","tag":"proxy","server":"127.0.0.1","server_port":10085,"uuid":"u","packet_encoding":"xudp","detour":"ssh-hop"},
    {"type":"ssh","tag":"ssh-hop","server":"h.invalid","server_port":4422,"user":"tunnel"},
    {"type":"vless","tag":"reality","server":"h.invalid","server_port":443,"uuid":"u","flow":"xtls-rprx-vision"},
    {"type":"direct","tag":"direct"}
  ]})";

static const char *kChainOnly = R"({
  "remarks": "client",
  "outbounds": [
    {"type":"vless","tag":"proxy","server":"127.0.0.1","server_port":10085,"uuid":"u","detour":"ssh-hop"},
    {"type":"ssh","tag":"ssh-hop","server":"h.invalid","server_port":4422,"user":"tunnel"}
  ]})";

static void testChainAndReality() {
    resetWorld();
    RawUpdater u;
    importString(u, kChainAndReality);

    // chain first, then the standalone profile, then the hops
    CHECK_EQ(groupOf(), (QList<int>{4, 1, 2, 3}));
    CHECK_EQ(names(), (QStringList{"client", "client / reality", "client / ssh", "client / vless"}));
    CHECK_EQ(ent(4)->type, QString("chain"));
    CHECK_EQ(ent(1)->type, QString("vless"));
    CHECK_EQ(ent(2)->type, QString("ssh"));
    CHECK_EQ(ent(3)->type, QString("vless"));
    // ssh is dialed first, the inner vless is the end of the chain
    CHECK_EQ(chainList(4), (QList<int>{2, 3}));
    CHECK_EQ(ent(4)->chain_link, false);
    CHECK_EQ(ent(1)->chain_link, false);
    CHECK_EQ(ent(2)->chain_link, true);
    CHECK_EQ(ent(3)->chain_link, true);
    CHECK(ent(4)->display_type.contains("(main)"));
    CHECK(ent(1)->display_type.isEmpty());
    CHECK_EQ(lastUnits, 1);
    // the standalone profile kept its own fields
    CHECK_EQ(ent(1)->serverAddress, QString("h.invalid"));
    CHECK_EQ(ent(1)->serverPort, 443);
    CHECK_EQ(ent(1)->ChainBean() == nullptr, true);
    CHECK_EQ(ent(3)->serverAddress, QString("127.0.0.1"));
    CHECK_EQ(ent(3)->serverPort, 10085);
    CHECK_EQ(u.proxies.size(), 4);
    // the group was written out in its final order
    CHECK(Configs::profileManager->GetGroup(1)->saves >= 1);
    CHECK_EQ(Configs::profileManager->lockDepth, 0);
    CHECK(Configs::profileManager->maxLockDepth >= 1);
}

static void testChainWithoutReality() {
    resetWorld();
    RawUpdater u;
    importString(u, kChainOnly);
    CHECK_EQ(groupOf(), (QList<int>{3, 1, 2}));
    CHECK_EQ(names(), (QStringList{"client", "client / ssh", "client / vless"}));
    CHECK_EQ(chainList(3), (QList<int>{1, 2}));
    CHECK_EQ(ent(1)->chain_link, true);
    CHECK_EQ(ent(2)->chain_link, true);
    CHECK_EQ(ent(3)->chain_link, false);
}

static void testArrayElementKeepsRemarks() {
    resetWorld();
    RawUpdater u;
    importString(u, kChainAndReality, true);
    CHECK_EQ(names(), (QStringList{"client", "client / reality", "client / ssh", "client / vless"}));
}

static void testResolvedChainRunsSshFirst() {
    // BuildChain resolves the list with the last entry first: that one becomes
    // the "proxy" outbound, every earlier entry is its detour.
    resetWorld();
    RawUpdater u;
    importString(u, kChainAndReality);
    auto status = std::make_shared<Configs::BuildConfigStatus>();
    auto resolved = Configs::ResolveChainInternal(status, ent(4));
    CHECK(status->result->error.isEmpty());
    CHECK_EQ(resolved.size(), 2);
    if (resolved.size() == 2) {
        CHECK_EQ(resolved[0]->type, QString("vless")); // exit, tagged "proxy"
        CHECK_EQ(resolved[0]->serverPort, 10085);
        CHECK_EQ(resolved[1]->type, QString("ssh")); // reached through nothing
    }
}

static void testNoDetourStaysIndependent() {
    resetWorld();
    RawUpdater u;
    importString(u, R"({"remarks":"r","outbounds":[
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u"},
        {"type":"ssh","tag":"b","server":"b.invalid","server_port":22,"user":"x"},
        {"type":"direct","tag":"direct"}]})");
    CHECK_EQ(groupOf(), (QList<int>{1, 2}));
    // several outbounds: named by their tags, as before
    CHECK_EQ(names(), (QStringList{"a", "b"}));
    CHECK_EQ(ent(1)->chain_link, false);
    CHECK_EQ(ent(2)->chain_link, false);
    CHECK_EQ(lastUnits, 0);
    CHECK(ent(1)->display_type.isEmpty());

    // a single outbound takes the name that was passed in, as before
    resetWorld();
    RawUpdater single;
    importString(single, R"({"remarks":"only","outbounds":[
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u"}]})", true);
    CHECK_EQ(names(), (QStringList{"only"}));
    // ...and without a name, its tag
    resetWorld();
    RawUpdater untitled;
    importString(untitled, R"({"remarks":"only","outbounds":[
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u"}]})", false);
    CHECK_EQ(names(), (QStringList{"a"}));
}

static void testUnusableDetoursAreIgnored() {
    // direct, an unknown tag, a tag that is not a profile and itself
    resetWorld();
    RawUpdater u;
    importString(u, R"({"outbounds":[
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u","detour":"direct"},
        {"type":"vless","tag":"b","server":"b.invalid","server_port":443,"uuid":"u","detour":"nowhere"},
        {"type":"vless","tag":"c","server":"c.invalid","server_port":443,"uuid":"u","detour":"c"},
        {"type":"vless","tag":"d","server":"d.invalid","server_port":443,"uuid":"u","detour":"gone"},
        {"type":"direct","tag":"direct"},
        {"type":"vless","tag":"gone","unparsable":true,"server":"x","server_port":1}]})");
    CHECK_EQ(names(), (QStringList{"a", "b", "c", "d"}));
    CHECK_EQ(lastUnits, 0);
    for (int id : groupOf()) CHECK_EQ(ent(id)->chain_link, false);
    CHECK(logged("\"nowhere\" is not a profile"));
    CHECK(logged("points at itself"));
    CHECK(!logged("\"direct\" is not a profile"));
}

static void testLoopsDoNotChain() {
    resetWorld();
    RawUpdater u;
    importString(u, R"({"outbounds":[
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u","detour":"b"},
        {"type":"vless","tag":"b","server":"b.invalid","server_port":443,"uuid":"u","detour":"c"},
        {"type":"vless","tag":"c","server":"c.invalid","server_port":443,"uuid":"u","detour":"a"},
        {"type":"vless","tag":"d","server":"d.invalid","server_port":443,"uuid":"u"}]})");
    CHECK_EQ(names(), (QStringList{"a", "b", "c", "d"}));
    CHECK_EQ(lastUnits, 0);
    CHECK(logged("detour loop"));
    for (int id : groupOf()) CHECK_EQ(ent(id)->chain_link, false);
}

static void testLongChainAndSharedHop() {
    resetWorld();
    RawUpdater u;
    // a -> b -> c, and e -> c: two chains over one shared last-mile hop
    importString(u, R"({"remarks":"r","outbounds":[
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u","detour":"b"},
        {"type":"trojan","tag":"b","server":"b.invalid","server_port":443,"password":"p","detour":"c"},
        {"type":"ssh","tag":"c","server":"c.invalid","server_port":22,"user":"x"},
        {"type":"vless","tag":"e","server":"e.invalid","server_port":443,"uuid":"u","detour":"c"}]})");
    // a, the first outbound nothing is dialed through, is the main one
    int chainA = 0, chainE = 0, hopC = 0;
    for (int id : groupOf()) {
        if (ent(id)->type == "chain" && ent(id)->name == "r") chainA = id;
        if (ent(id)->type == "chain" && ent(id)->name == "r / e") chainE = id;
        if (ent(id)->type == "ssh") hopC = id;
    }
    CHECK(chainA > 0 && chainE > 0 && hopC > 0);
    if (chainA <= 0 || chainE <= 0 || hopC <= 0) return;
    CHECK_EQ(groupOf().first(), chainA);
    CHECK_EQ(groupOf().size(), 6); // 2 chains + hops a, b, c, e
    int typeSsh = 0;
    for (int id : groupOf()) typeSsh += ent(id)->type == "ssh";
    CHECK_EQ(typeSsh, 1); // the shared hop is stored once
    auto listA = chainList(chainA);
    auto listE = chainList(chainE);
    CHECK_EQ(listA.size(), 3);
    CHECK_EQ(listE.size(), 2);
    if (listA.size() == 3 && listE.size() == 2) {
        CHECK_EQ(ent(listA[0])->type, QString("ssh")); // dialed first
        CHECK_EQ(ent(listA[1])->type, QString("trojan"));
        CHECK_EQ(ent(listA[2])->type, QString("vless"));
        CHECK_EQ(listE[0], listA[0]);
        CHECK_EQ(ent(listE[1])->name, QString("r / e / vless"));
    }
    CHECK_EQ(ent(chainA)->display_type.contains("(main)"), true);
    CHECK_EQ(ent(chainE)->display_type.contains("(main)"), false);
    CHECK_EQ(ent(chainE)->chain_link, false);
    CHECK_EQ(ent(hopC)->name, QString("r / ssh"));
}

static void testMainEntrySelection() {
    // the first outbound nothing is dialed through leads, whatever its tag:
    // "proxy" means nothing special
    resetWorld();
    RawUpdater u;
    importString(u, R"({"remarks":"r","outbounds":[
        {"type":"vless","tag":"x","server":"x.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"ssh","tag":"h","server":"h.invalid","server_port":22,"user":"x"},
        {"type":"vless","tag":"proxy","server":"p.invalid","server_port":443,"uuid":"u"}]})");
    CHECK_EQ(ent(groupOf().first())->type, QString("chain"));
    CHECK(ent(groupOf().first())->display_type.contains("(main)"));
    CHECK_EQ(names().mid(0, 2), (QStringList{"r", "r / proxy"}));

    // a plain profile first in the input leads a chain behind it; only a
    // chain is labelled
    resetWorld();
    RawUpdater v;
    importString(v, R"({"remarks":"r","outbounds":[
        {"type":"vless","tag":"plain","server":"p.invalid","server_port":443,"uuid":"u"},
        {"type":"vless","tag":"x","server":"x.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"ssh","tag":"h","server":"h.invalid","server_port":22,"user":"x"}]})");
    CHECK_EQ(ent(groupOf().first())->type, QString("vless"));
    CHECK_EQ(ent(groupOf().first())->serverAddress, QString("p.invalid"));
    CHECK(ent(groupOf().first())->display_type.isEmpty());
    CHECK_EQ(names().mid(0, 2), (QStringList{"r", "r / x"}));
    for (int id : groupOf()) {
        if (ent(id)->type == "chain") CHECK(!ent(id)->display_type.contains("(main)"));
    }

    // a hop listed first does not count: the first outbound that is not a hop
    resetWorld();
    RawUpdater w;
    importString(w, R"({"remarks":"r","outbounds":[
        {"type":"ssh","tag":"h","server":"h.invalid","server_port":22,"user":"x"},
        {"type":"vless","tag":"x","server":"x.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"vless","tag":"plain","server":"p.invalid","server_port":443,"uuid":"u"}]})");
    CHECK_EQ(ent(groupOf().first())->type, QString("chain"));
    CHECK_EQ(names().mid(0, 2), (QStringList{"r", "r / plain"}));
}

static void testChainsCanBeSwitchedOff() {
    resetWorld();
    RawUpdater u;
    u.allow_chains = false;
    importString(u, kChainAndReality);
    CHECK_EQ(names(), (QStringList{"proxy", "ssh-hop", "reality"}));
    CHECK_EQ(lastUnits, 0);
    for (int id : groupOf()) CHECK_EQ(ent(id)->chain_link, false);
}

static void testNoTagsStillWorks() {
    resetWorld();
    RawUpdater u;
    importString(u, R"({"outbounds":[
        {"type":"vless","server":"a.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"ssh","tag":"h","server":"h.invalid","server_port":22,"user":"x"}]})");
    // an untagged outbound can still be the end of a chain
    CHECK_EQ(groupOf().size(), 3);
    CHECK_EQ(ent(groupOf().first())->type, QString("chain"));
    CHECK(!ent(groupOf().first())->name.isEmpty());
}

// ---- subscriptions ------------------------------------------------------

// Mirrors GroupUpdater::Update(): the ignore map is filled with what the group
// holds, the input is parsed, whatever stayed unclaimed is dropped.
struct SubscriptionRun {
    RawUpdater u;
    QList<int> dropped;
    void run(int gid, const char *text, bool chains = true) {
        runMany(gid, {text}, chains);
    }
    void runMany(int gid, std::initializer_list<const char *> texts, bool chains = true) {
        u.gid_add_to = gid;
        u.allow_chains = chains;
        for (int id : Configs::profileManager->GetGroup(gid)->profiles) {
            auto profile = Configs::profileManager->GetProfile(id);
            u.ignore_map[Configs::ProfileFilterKey(profile, false)] = false;
        }
        for (const char *text : texts) {
            auto obj = parse(text);
            u.updateSingBox(obj, obj["remarks"].toString());
        }
        Configs::profileManager->AddProfileBatch(u.proxies, u.gid_add_to);
        u.FinalizeChains();
        for (auto it = u.ignore_map.begin(); it != u.ignore_map.end(); ++it) {
            if (!it.value()) dropped << it.key().key->id;
        }
        auto group = Configs::profileManager->GetGroup(gid);
        for (int id : dropped) {
            group->profiles.removeAll(id);
            Configs::profileManager->ents.erase(id);
        }
    }
};

static void testSubscriptionKeepsWhatDidNotChange() {
    resetWorld();
    { SubscriptionRun first; first.run(2, kChainAndReality); }
    auto before = groupOf(2);
    CHECK_EQ(before, (QList<int>{4, 1, 2, 3}));
    auto chainBefore = chainList(4);

    SubscriptionRun again;
    again.run(2, kChainAndReality);
    CHECK_EQ(groupOf(2), before);
    CHECK_EQ(chainList(4), chainBefore);
    CHECK(again.u.proxies.isEmpty());
    CHECK(again.dropped.isEmpty());
    CHECK_EQ(Configs::profileManager->maxId, 4); // nothing new was created
}

static const char *kChangedHop = R"({
  "remarks": "client",
  "outbounds": [
    {"type":"vless","tag":"proxy","server":"127.0.0.1","server_port":10085,"uuid":"u","packet_encoding":"xudp","detour":"ssh-hop"},
    {"type":"ssh","tag":"ssh-hop","server":"h.invalid","server_port":4422,"user":"tunnel","host_key":["new key"]},
    {"type":"vless","tag":"reality","server":"h.invalid","server_port":443,"uuid":"u","flow":"xtls-rprx-vision"}
  ]})";

static void testSubscriptionKeepsChainIdWhenAHopChanges() {
    resetWorld();
    { SubscriptionRun first; first.run(2, kChainAndReality); }
    CHECK_EQ(groupOf(2), (QList<int>{4, 1, 2, 3}));

    SubscriptionRun update;
    update.run(2, kChangedHop);
    // the chain is the same profile as before, now over the new ssh hop
    CHECK(groupOf(2).contains(4));
    CHECK_EQ(groupOf(2).first(), 4);
    if (!groupOf(2).contains(4) || ent(4) == nullptr) return;
    CHECK_EQ(ent(4)->name, QString("client"));
    CHECK_EQ(ent(4)->type, QString("chain"));
    auto list = chainList(4);
    CHECK_EQ(list.size(), 2);
    if (list.size() == 2) {
        CHECK(ent(list[0]) != nullptr);
        CHECK(ent(list[1]) != nullptr);
        CHECK_EQ(ent(list[0])->type, QString("ssh"));
        CHECK(list[0] != 2); // the old ssh hop is gone
        CHECK_EQ(list[1], 3); // the unchanged inner vless is reused
    }
    // the old hop was dropped, nothing the chain points at is missing
    CHECK(update.dropped.contains(2));
    CHECK(!update.dropped.contains(4));
    CHECK(!update.dropped.contains(1));
    CHECK(!update.dropped.contains(3));
    for (int id : chainList(4)) CHECK(groupOf(2).contains(id));
    int chains = 0;
    for (int id : groupOf(2)) chains += ent(id)->type == "chain";
    CHECK_EQ(chains, 1);
    CHECK(update.u.ignore_map.size() >= 4);
}

static void testSubscriptionDropsAChainThatIsGone() {
    resetWorld();
    { SubscriptionRun first; first.run(2, kChainAndReality); }
    SubscriptionRun update;
    update.run(2, R"({"remarks":"client","outbounds":[
        {"type":"vless","tag":"reality","server":"h.invalid","server_port":443,"uuid":"u","flow":"xtls-rprx-vision"}]})");
    // only one outbound left: no chain, the plain profile is kept as it was
    CHECK_EQ(groupOf(2), (QList<int>{1}));
    CHECK(update.dropped.contains(4));
    CHECK(update.dropped.contains(2));
    CHECK(update.dropped.contains(3));
}

static void testTwoClientsInOneGroupStayApart() {
    // the same ports and hop layout twice: nothing may merge or steal a hop
    resetWorld();
    RawUpdater a;
    importString(a, kChainOnly);
    RawUpdater b;
    importString(b, R"({"remarks":"other","outbounds":[
      {"type":"vless","tag":"proxy","server":"127.0.0.1","server_port":10085,"uuid":"v","detour":"ssh-hop"},
      {"type":"ssh","tag":"ssh-hop","server":"h.invalid","server_port":4422,"user":"someone"}]})");
    CHECK_EQ(groupOf().size(), 6);
    int chains = 0;
    for (int id : groupOf()) chains += ent(id)->type == "chain";
    CHECK_EQ(chains, 2);
    QList<int> l1 = chainList(3), l2 = chainList(6);
    for (int x : l1) CHECK(!l2.contains(x));
}

// ---- profile key --------------------------------------------------------

static void testHopsAndChainsAreNotKeyedByAddress() {
    auto a = Configs::ProfileManager::NewProxyEntity("chain");
    auto b = Configs::ProfileManager::NewProxyEntity("chain");
    a->unlock(a->ChainBean())->list = {1, 2};
    b->unlock(b->ChainBean())->list = {3, 4};
    CHECK(Configs::ProfileFilter_ent_key(a, true) != Configs::ProfileFilter_ent_key(b, true));
    CHECK(Configs::ProfileFilter_ent_key(a, true) == Configs::ProfileFilter_ent_key(a, true));

    auto h1 = Configs::ProfileManager::NewProxyEntity("vless");
    auto h2 = Configs::ProfileManager::NewProxyEntity("vless");
    h1->serverAddress = h2->serverAddress = "127.0.0.1";
    h1->serverPort = h2->serverPort = 10085;
    h1->unlock(h1->bean())->TryParseJson(QJsonObject{{"uuid", "one"}});
    h2->unlock(h2->bean())->TryParseJson(QJsonObject{{"uuid", "two"}});
    // ordinary profiles on one address are duplicates for "by address"...
    CHECK(Configs::ProfileFilter_ent_key(h1, true) == Configs::ProfileFilter_ent_key(h2, true));
    // ...hops are not
    h1->chain_link = h2->chain_link = true;
    CHECK(Configs::ProfileFilter_ent_key(h1, true) != Configs::ProfileFilter_ent_key(h2, true));
}

// ---- review of the first version ---------------------------------------

static int chainEndingAt(const QString &server, int gid = 1) {
    for (int id : groupOf(gid)) {
        if (ent(id)->type != "chain") continue;
        auto list = chainList(id);
        if (!list.isEmpty() && ent(list.last()) && ent(list.last())->serverAddress == server) return id;
    }
    return -1;
}

static void testReusedProfilesTakeTheirNewRole() {
    // a subscription imported before chains existed, then updated
    resetWorld();
    { SubscriptionRun old; old.run(2, kChainOnly, false); }
    CHECK_EQ(groupOf(2), (QList<int>{1, 2}));
    CHECK_EQ(ent(1)->chain_link, false);
    CHECK_EQ(ent(2)->chain_link, false);

    SubscriptionRun update;
    update.run(2, kChainOnly);
    int chain = chainEndingAt("127.0.0.1", 2);
    CHECK(chain > 0);
    if (chain <= 0) return;
    CHECK_EQ(chainList(chain), (QList<int>{2, 1})); // the old profiles are reused
    CHECK_EQ(ent(1)->chain_link, true);
    CHECK_EQ(ent(2)->chain_link, true);
    CHECK(ent(1)->saves > 0); // and stored with their new role
    CHECK(ent(2)->saves > 0);

    // the ssh hop is now offered on its own: it is no hop any more
    SubscriptionRun standalone;
    standalone.run(2, R"j({"remarks":"client","outbounds":[
        {"type":"ssh","tag":"ssh-hop","server":"h.invalid","server_port":4422,"user":"tunnel"}]})j");
    CHECK_EQ(groupOf(2), (QList<int>{2}));
    if (ent(2)) CHECK_EQ(ent(2)->chain_link, false);
}

static void testChainIdFollowsItsExitWhenReordered() {
    resetWorld();
    { SubscriptionRun first; first.run(2, R"j({"remarks":"r","outbounds":[
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"vless","tag":"b","server":"b.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"ssh","tag":"h","server":"h.invalid","server_port":22,"user":"x"}]})j"); }
    const int a = chainEndingAt("a.invalid", 2), b = chainEndingAt("b.invalid", 2);
    CHECK(a > 0 && b > 0 && a != b);
    if (a <= 0 || b <= 0) return;
    CHECK_EQ(ent(a)->name, QString("r"));

    // b moves first, and the shared hop changes
    SubscriptionRun update;
    update.run(2, R"j({"remarks":"r","outbounds":[
        {"type":"vless","tag":"b","server":"b.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"ssh","tag":"h","server":"h.invalid","server_port":22,"user":"x","host_key":["new"]}]})j");
    CHECK_EQ(chainEndingAt("a.invalid", 2), a);
    CHECK_EQ(chainEndingAt("b.invalid", 2), b);
    if (ent(b)) {
        CHECK_EQ(ent(b)->name, QString("r")); // b is the main entry now
        CHECK(ent(b)->display_type.contains("(main)"));
    }
    if (ent(a)) {
        CHECK_EQ(ent(a)->name, QString("r / a"));
        CHECK(!ent(a)->display_type.contains("(main)"));
    }

    // only the order changes: the chains are kept and relabelled
    SubscriptionRun back;
    back.run(2, R"j({"remarks":"r","outbounds":[
        {"type":"vless","tag":"a","server":"a.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"vless","tag":"b","server":"b.invalid","server_port":443,"uuid":"u","detour":"h"},
        {"type":"ssh","tag":"h","server":"h.invalid","server_port":22,"user":"x","host_key":["new"]}]})j");
    CHECK_EQ(chainEndingAt("a.invalid", 2), a);
    CHECK_EQ(chainEndingAt("b.invalid", 2), b);
    if (ent(a)) CHECK_EQ(ent(a)->name, QString("r"));
    if (ent(b)) CHECK(!ent(b)->display_type.contains("(main)"));
}

// what "remove duplicates" and the subscription option of the same name do
static QList<int> duplicatesToDelete(bool by_address) {
    QList<Ent> all, out, del;
    for (int id : groupOf()) all << ent(id);
    Configs::ProfileFilter::Uniq(all, out, by_address, false);
    Configs::ProfileFilter::OnlyInSrc_ByPointer(all, out, del);
#ifdef HAS_OTHER_GROUPS
    // the main window adds the chains of the other groups once there is
    // something to delete
    if (!del.isEmpty()) all += ChainsOfOtherGroups(1);
#endif
#ifdef HAS_KEEP_HOPS
    Configs::ProfileFilter::KeepReferencedHops(all, del);
#endif
    QList<int> ids;
    for (const auto &e : del) ids << e->id;
    return ids;
}

static void testDuplicateCleanupKeepsReferencedHops() {
    for (bool by_address : {true, false}) {
        resetWorld();
        RawUpdater a, b;
        importString(a, kChainOnly);
        importString(b, kChainOnly); // the same string pasted twice
        CHECK_EQ(groupOf().size(), 6);
        for (int id : duplicatesToDelete(by_address)) {
            Configs::profileManager->GetGroup(1)->profiles.removeAll(id);
            Configs::profileManager->ents.erase(id);
        }
        for (int id : groupOf()) {
            if (ent(id)->type != "chain") continue;
            for (int hop : chainList(id)) CHECK(ent(hop) != nullptr); // nothing dangles
        }
    }
}

// ---- second round of review ------------------------------------------

static int chainStartingAt(const QString &server, int gid = 2) {
    for (int id : groupOf(gid)) {
        if (ent(id)->type != "chain") continue;
        auto list = chainList(id);
        if (!list.isEmpty() && ent(list.first()) && ent(list.first())->serverAddress == server) return id;
    }
    return -1;
}

static const char *kEntryA = R"j({"outbounds":[
    {"type":"vless","tag":"proxy","server":"127.0.0.1","server_port":10085,"uuid":"a","detour":"ssh"},
    {"type":"ssh","tag":"ssh","server":"a.invalid","server_port":22,"user":"a"}]})j";
static const char *kEntryAChanged = R"j({"outbounds":[
    {"type":"vless","tag":"proxy","server":"127.0.0.1","server_port":10085,"uuid":"a","detour":"ssh"},
    {"type":"ssh","tag":"ssh","server":"a.invalid","server_port":22,"user":"a","host_key":["new"]}]})j";
static const char *kEntryB = R"j({"outbounds":[
    {"type":"vless","tag":"proxy","server":"127.0.0.1","server_port":10085,"uuid":"b","detour":"ssh"},
    {"type":"ssh","tag":"ssh","server":"b.invalid","server_port":22,"user":"b"}]})j";

static int chainCount(int gid) {
    int n = 0;
    for (int id : groupOf(gid)) n += ent(id)->type == "chain";
    return n;
}

static void testEqualKeysInOneUpdateDoNotTakeAChain() {
    // two inputs of one subscription with the same remarks and tags; the one
    // that did not change keeps its chain
    resetWorld();
    { SubscriptionRun first; first.run(2, kEntryA); }
    const int a = chainStartingAt("a.invalid");
    CHECK(a > 0);
    SubscriptionRun update;
    update.runMany(2, {kEntryB, kEntryA});
    CHECK_EQ(chainStartingAt("a.invalid"), a);
    CHECK(chainStartingAt("b.invalid") > 0);
    CHECK(chainStartingAt("b.invalid") != a);
    CHECK_EQ(chainCount(2), 2);
    CHECK(!update.dropped.contains(a));

    // both changed: either may be meant, so neither takes the old id
    resetWorld();
    { SubscriptionRun first; first.run(2, kEntryA); }
    const int old = chainStartingAt("a.invalid");
    SubscriptionRun both;
    both.runMany(2, {kEntryB, kEntryAChanged});
    CHECK(chainStartingAt("b.invalid") != old);
    CHECK_EQ(chainCount(2), 2);
    for (int id : groupOf(2)) {
        if (ent(id)->type != "chain") continue;
        for (int hop : chainList(id)) CHECK(ent(hop) != nullptr);
    }
}

static void testHopsOfChainsInOtherGroupsAreKept() {
    // the same chain imported twice, then the second chain moved elsewhere
    resetWorld();
    RawUpdater a, b;
    importString(a, kChainOnly);
    importString(b, kChainOnly);
    int moved = -1;
    for (int id : groupOf()) {
        if (ent(id)->type == "chain") moved = id;
    }
    CHECK(moved > 0);
    if (moved <= 0) return;
    Configs::profileManager->GetGroup(1)->profiles.removeAll(moved);
    Configs::profileManager->GetGroup(2)->profiles << moved;
    ent(moved)->gid = 2;
    for (int id : duplicatesToDelete(true)) {
        Configs::profileManager->GetGroup(1)->profiles.removeAll(id);
        Configs::profileManager->ents.erase(id);
    }
    for (int hop : chainList(moved)) CHECK(ent(hop) != nullptr);
}

static void testProxyTaggedDirectCanCarryAChain() {
    resetWorld();
    RawUpdater u;
    importString(u, R"j({"remarks":"r","outbounds":[
        {"type":"vless","tag":"x","server":"127.0.0.1","server_port":10085,"uuid":"u","detour":"direct"},
        {"type":"ssh","tag":"direct","server":"h.invalid","server_port":22,"user":"x"}]})j");
    CHECK_EQ(groupOf().size(), 3);
    if (groupOf().size() != 3) return;
    CHECK_EQ(ent(groupOf().first())->type, QString("chain"));
    if (ent(groupOf().first())->type != "chain") return;
    auto list = chainList(groupOf().first());
    CHECK_EQ(list.size(), 2);
    if (list.size() == 2) {
        CHECK_EQ(ent(list[0])->type, QString("ssh"));
        CHECK_EQ(ent(list[1])->serverPort, 10085);
    }
}

int main() {
    Configs::profileManager->groups[1] = std::make_shared<Configs::Group>();
    testChainAndReality();
    testChainWithoutReality();
    testArrayElementKeepsRemarks();
    testResolvedChainRunsSshFirst();
    testNoDetourStaysIndependent();
    testUnusableDetoursAreIgnored();
    testLoopsDoNotChain();
    testLongChainAndSharedHop();
    testMainEntrySelection();
    testChainsCanBeSwitchedOff();
    testNoTagsStillWorks();
    testSubscriptionKeepsWhatDidNotChange();
    testSubscriptionKeepsChainIdWhenAHopChanges();
    testSubscriptionDropsAChainThatIsGone();
    testTwoClientsInOneGroupStayApart();
    testHopsAndChainsAreNotKeyedByAddress();
    testReusedProfilesTakeTheirNewRole();
    testChainIdFollowsItsExitWhenReordered();
    testDuplicateCleanupKeepsReferencedHops();
    testProxyTaggedDirectCanCarryAChain();
    testEqualKeysInOneUpdateDoNotTakeAChain();
    testHopsOfChainsInOtherGroupsAreKept();
    std::cout << "RESULT " << (checks - failures) << "/" << checks << " checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
