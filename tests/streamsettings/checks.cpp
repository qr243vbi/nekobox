// Synthetic text only. This is deliberately not a valid network ECH config.
const QString syntheticConfig = "-----BEGIN ECHCONFIG-----\nsynthetic-config-only\n-----END ECHCONFIG-----\n";
int failures = 0, assertions = 0;
void check(const char *label, bool condition) {
    ++assertions;
    std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
    if (!condition) ++failures;
}
QJsonObject snapshot(const JsonStore &source) {
#ifdef USE_REAL_QT
    // Exercise real Qt JSON byte encoding/parsing when Qt6 is available.
    return QJsonDocument::fromJson(QJsonDocument(source.ToJson()).toJson()).object();
#else
    return source.ToJson();
#endif
}
int main() {
    Configs::V2rayStreamSettings defaults;
    check("legacy defaults disable ECH", !defaults.enable_ech);
    check("legacy defaults leave ECH config empty", defaults.ech_config == "");
    check("legacy defaults leave query name empty", defaults.query_server_name == "");

    Configs::V2rayStreamSettings edited;
    edited.enable_ech = true;
    edited.ech_config = syntheticConfig;
    edited.query_server_name = "public.example.invalid";
    edited.sni = "server.example.invalid";
    const auto saved = snapshot(edited);
    check("serialized enable_ech is a true boolean", saved["enable_ech"].isBool() && saved["enable_ech"].toBool());
    check("serialized ech_config preserves exact multiline text", saved["ech_config"].isString() && saved["ech_config"].toString() == syntheticConfig);
    check("serialized query_server_name preserves imported value", saved["query_server_name"].isString() && saved["query_server_name"].toString() == edited.query_server_name);

    Configs::V2rayStreamSettings reopened;
    reopened.FromJson(saved);
    check("save/reopen preserves enabled checkbox value", reopened.enable_ech);
    check("save/reopen preserves ECH config", reopened.ech_config == syntheticConfig);
    check("save/reopen preserves query server name", reopened.query_server_name == edited.query_server_name);
    check("existing SNI roundtrip remains intact", reopened.sni == edited.sni);

    reopened.enable_ech = false;
    const auto savedDisabled = snapshot(reopened);
    check("serialized enable_ech is an explicit false boolean", savedDisabled["enable_ech"].isBool() && !savedDisabled["enable_ech"].toBool());
    Configs::V2rayStreamSettings disabled;
    disabled.enable_ech = true;
    disabled.FromJson(savedDisabled);
    check("disabling ECH persists false", !disabled.enable_ech);
    check("disabling ECH preserves editable config", disabled.ech_config == syntheticConfig);
    check("disabling ECH preserves imported query name", disabled.query_server_name == edited.query_server_name);

    Configs::V2rayStreamSettings cleared;
    cleared.ech_config = syntheticConfig;
    cleared.query_server_name = "previous.example.invalid";
    cleared.FromJson(snapshot(defaults));
    check("empty ECH config clears a previously populated value", cleared.ech_config == "");
    check("empty query name clears a previously populated value", cleared.query_server_name == "");

    Configs::V2rayStreamSettings typed;
    typed.enable_ech = true;
    typed.ech_config = syntheticConfig;
    typed.query_server_name = "typed.example.invalid";
    QJsonObject wrongTypes;
    wrongTypes.insert("enable_ech", "false");
    wrongTypes.insert("ech_config", false);
    wrongTypes.insert("query_server_name", 42);
    typed.FromJson(wrongTypes);
    check("string boolean does not overwrite enabled ECH", typed.enable_ech);
    check("boolean config does not overwrite existing text", typed.ech_config == syntheticConfig);
    check("numeric query name does not overwrite existing text", typed.query_server_name == "typed.example.invalid");

    QJsonObject legacy;
    legacy.insert("sni", "legacy.example.invalid");
    Configs::V2rayStreamSettings oldProfile;
    oldProfile.FromJson(legacy);
    check("old profile without ECH keys keeps default disabled", !oldProfile.enable_ech);
    check("old profile without ECH keys keeps empty config", oldProfile.ech_config == "");
    check("old profile without ECH keys keeps empty query name", oldProfile.query_server_name == "");
    check("old profile still loads existing SNI", oldProfile.sni == "legacy.example.invalid");

    Configs::V2rayStreamSettings second;
    second.enable_ech = true;
    second.ech_config = "second independent value";
    second.query_server_name = "second.example.invalid";
    Configs::V2rayStreamSettings secondReopened;
    secondReopened.FromJson(snapshot(second));
    check("cached field map reads the second instance's config", secondReopened.ech_config == second.ech_config);
    check("cached field map reads the second instance's query name", secondReopened.query_server_name == second.query_server_name);
    check("reloading another instance leaves the first intact", edited.enable_ech && edited.ech_config == syntheticConfig);
    std::cout << "SUMMARY assertions=" << assertions << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
