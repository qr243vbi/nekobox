int total = 0, failures = 0;
void check(const char *name, bool ok) {
    ++total;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
}
int main() {
    registerFixtures();
    using Subscription::RawUpdater;
    for (const auto &text : {outbound, whitespace, extended}) {
        RawUpdater updater;
        updater.update(text);
        check("standalone object added exactly once", updater.proxies.size() == 1);
        if (updater.proxies.size() == 1) {
            auto ent = updater.proxies[0];
            check("standalone profile retains custom type", ent->type == "custom");
            check("standalone profile retains internal core", ent->storage->core == "internal");
            check("standalone retains exact original JSON text", ent->storage->config_simple == text);
            auto result = ent->storage->BuildCoreObjSingBox();
            check("actual custom builder reads original outbound type", result.outbound["type"].toString() == QString2QJsonObject(text)["type"].toString());
            check("actual custom builder retains server", result.outbound["server"].toString() == "127.0.0.1");
        }
    }
    for (const auto &text : {missing_type,null_type,number_type,bool_type,array_type,object_type,empty_type,broken,empty,scalar}) {
        RawUpdater updater;
        updater.update(text);
        check("invalid or unsupported standalone JSON remains rejected", updater.proxies.empty());
    }
    for (const auto &text : {array_string,array_wrapper}) {
        RawUpdater updater;
        updater.update(text);
        check("JSON array string/proxy wrapper imports standalone JSON", updater.proxies.size() == 1 && updater.proxies[0]->storage->config_simple == outbound);
    }
    {
        RawUpdater updater;
        updater.update(array_outbound);
        check("bare outbound object in array retains existing unsupported behavior", updater.proxies.empty() && updater.envelopes.empty());
    }
    for (const auto &text : {envelope,endpoints,array_envelope}) {
        RawUpdater updater;
        updater.update(text);
        check("outbound/endpoint envelope stays on native envelope dispatcher", updater.envelopes.size() == 1 && updater.proxies.empty());
        if (text == array_envelope) check("array envelope name preserved", updater.envelopeNames[0] == "Envelope test");
    }
    for (const auto &text : {fullconfig,routeconfig,dnsconfig,array_fullconfig}) {
        RawUpdater updater;
        auto before = sanitizeCalls;
        updater.update(text);
        check("full config added exactly once", updater.proxies.size() == 1 && updater.envelopes.empty());
        check("full config still uses sanitizer", sanitizeCalls == before + 1);
        if (updater.proxies.size() == 1) {
            check("full config retains internal-full mode", updater.proxies[0]->storage->core == "internal-full");
            check("full config name retained", !updater.proxies[0]->name.isEmpty());
        }
    }
    {
        RawUpdater updater;
        updater.update(sip);
        check("SIP008 dispatcher unchanged", updater.sipCalls == 1 && updater.proxies.empty());
    }
    for (const auto &text : {sharelink,upperlink,nekoraylink}) {
        RawUpdater updater;
        auto before = fixCalls;
        updater.update(text);
        check("share-link dispatcher adds accepted parser result", updater.proxies.size() == 1);
        check("share-link fixup preserved", fixCalls == before + 1);
    }
    for (const auto &text : {badlink,unknownlink}) {
        RawUpdater updater;
        updater.update(text);
        check("rejected share-link parser result not added", updater.proxies.empty());
    }
    for (const auto &text : {mixed,multiline}) {
        RawUpdater updater;
        updater.update(text);
        check("multiple stack entries survive JSON addition", updater.proxies.size() == 3);
        if (updater.proxies.size() == 3) {
            check("stack entry ordering preserved", updater.proxies[0]->type == "custom" && updater.proxies[1]->type == "socks" && updater.proxies[2]->type == "custom");
            check("later custom entry retains own payload", updater.proxies[2]->storage->config_simple == outbound_second);
            check("earlier custom entry remains independently owned", updater.proxies[0] != updater.proxies[2] && updater.proxies[0]->storage->config_simple == outbound);
        }
    }
    {
        RawUpdater updater;
        auto ignored = Configs::ProfileManager::NewProxyEntity("custom");
        ignored->storage->config_simple = outbound;
        auto key = Configs::ProfileFilterKey(ignored, false);
        updater.ignore_map[key] = false;
        updater.update(mixed);
        check("JSON uses actual AddProxy ignore boundary", updater.ignore_map[key]);
        check("ignored JSON continues to distinct later entries", updater.proxies.size() == 2 && updater.proxies[0]->type == "socks" && updater.proxies[1]->storage->config_simple == outbound_second);
    }
    for (const auto &text : {comments,array_other}) {
        RawUpdater updater;
        updater.update(text);
        check("ignored entries do not consume following share link", updater.proxies.size() == 1 && updater.proxies[0]->type == "socks");
    }
    std::cout << "RESULT: " << total-failures << "/" << total << " checks passed; " << failures << " failed\n";
    return failures ? 1 : 0;
}
