int total = 0, failures = 0;
void check(bool ok, const char *label) {
    ++total;
    if (!ok) {
        ++failures;
        if (failures <= 30) std::cout << "FAIL " << label << '\n';
    }
}
QString outbound(const QString &value, bool pretty = false) {
    // Exercise both quoted keys and values, nested objects and arrays.
    QJsonObject obj{{"type", "socks"}, {"server", "127.0.0.1"},
                    {"server_port", 1080}, {"tag", value},
                    {"extra", QJsonArray{QJsonObject{{value, value}}}}};
    return QString::fromUtf8(QJsonDocument(obj).toJson(
        pretty ? QJsonDocument::Indented : QJsonDocument::Compact)).trimmed();
}
QList<QString> trimmedParts(const QString &input) {
    QList<QString> result;
    for (const auto &part : Subscription::Disect(input)) {
        if (!part.trimmed().isEmpty()) result << part.trimmed();
    }
    return result;
}
void splitterCase(const QString &value, bool pretty) {
    const auto first = outbound(value, pretty);
    const auto second = outbound("second", pretty);
    const auto stream = first + '\n' + second;
    check(Subscription::JsonEndIdx(stream, 0) == first.size() - 1,
          "string contents do not change first object boundary");
    check(trimmedParts(stream) == QList<QString>{first, second},
          "splitter preserves complete objects and their order");
    // Nonzero offsets are UTF-16 QString indexes, including a surrogate pair.
    const auto prefix = QString::fromUtf8("前😀\n");
    check(Subscription::JsonEndIdx(prefix + stream, prefix.size()) == prefix.size() + first.size() - 1,
          "object boundary respects nonzero Unicode prefix offset");
    for (const auto &part : trimmedParts(stream)) {
        QJsonParseError error;
        QJsonDocument::fromJson(part.toUtf8(), &error);
        check(error.error == QJsonParseError::NoError,
              "each split JSON object remains parseable by Qt");
    }
}
void importCase(const QString &value, bool pretty) {
    const auto first = outbound(value, pretty);
    const auto second = outbound("second", pretty);
    Subscription::RawUpdater updater;
    updater.update(first + "\nsocks://127.0.0.1:1080\n" + second);
    check(updater.proxies.size() == 3, "multiline import retains both JSON objects and middle share link");
    if (updater.proxies.size() == 3) {
        check(updater.proxies[0]->type == "custom" && updater.proxies[1]->type == "socks" &&
              updater.proxies[2]->type == "custom", "multiline import preserves profile order");
        check(updater.proxies[0]->storage->config_simple == first &&
              updater.proxies[2]->storage->config_simple == second,
              "multiline import retains exact JSON text independently");
        check(updater.proxies[0]->storage->core == "internal" &&
              updater.proxies[2]->storage->core == "internal", "JSON objects retain existing custom/internal mode");
    }
}
void malformedImportCase(const QString &bad, bool surrounded) {
    QJsonParseError error;
    QJsonDocument::fromJson(bad.toUtf8(), &error);
    check(error.error != QJsonParseError::NoError, "malformed multiline fixture is rejected by Qt");
    Subscription::RawUpdater updater;
    updater.update(surrounded ? outbound("before") + '\n' + bad + '\n' + outbound("after") : bad);
    check(updater.proxies.size() == (surrounded ? 2 : 0),
          "malformed multiline object terminates without adding a profile");
    if (surrounded && updater.proxies.size() == 2) {
        check(updater.proxies[0]->storage->config_simple == outbound("before") &&
              updater.proxies[1]->storage->config_simple == outbound("after"),
              "complete valid entries around malformed object survive in order");
    }
}
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    const QList<QString> focused{
        "plain", "{", "}", "}{", "{{nested}}", "\"}", "\\", "\\\"{", "\\\\\"}",
        "tail\\", "tail\\\\", "line\n}next", "tab\t{", QString::fromUtf8("节点😀}{"), ""
    };
    const QList<QString> malformed{
        R"({
"type":"socks",
"tag":"{",
})",
        R"({
"type":"socks",
"tag":"}",
})",
        R"({
"type":"socks",
})",
        " \n" + QString(R"({
"type":"socks",
"tag":"{",
})") + " \r\n",
        R"({
"type":"socks",
"extra":[1,]
})"
    };
    const int validCount = focused.size() * 2;
    const int importCount = validCount + malformed.size() * 2;
    if (argc == 2 && QString::fromUtf8(argv[1]) == "--count-imports") {
        std::cout << importCount << '\n';
        return 0;
    }
    std::cout << "Qt " << qVersion() << '\n';
    if (argc == 2) {
        bool ok;
        const int index = QString::fromUtf8(argv[1]).toInt(&ok);
        if (!ok || index < 0 || index >= importCount) return 2;
        if (index < validCount) importCase(focused[index / 2], index % 2);
        else malformedImportCase(malformed[(index - validCount) / 2], index % 2);
        std::cout << "RESULT: " << total - failures << "/" << total
                  << " import checks passed; " << failures << " failed\n";
        return failures ? 1 : 0;
    }
    for (const auto &value : focused) {
        for (bool pretty : {false, true}) {
            splitterCase(value, pretty);
        }
    }
    // Exhaustive small strings catch odd/even escaping and quote-state bugs.
    QList<QString> values{QString()};
    const QString alphabet = "a{}\"\\\n";
    for (int depth = 0; depth < 4; ++depth) {
        QList<QString> next;
        for (const auto &prefix : values) {
            for (const auto ch : alphabet) {
                const auto value = prefix + ch;
                splitterCase(value, false);
                next << value;
            }
        }
        values = next;
    }
    const auto first = outbound("first", true), second = outbound("second", true);
    check(trimmedParts(first + second) == QList<QString>{first, second},
          "adjacent objects retain existing splitter support");
    check(trimmedParts("\n\r\n" + first + "\r\n\n" + second + "\r\n") == QList<QString>{first, second},
          "CRLF and blank-line import normalization unchanged");
    check(Subscription::Disect("# { comment\n// } comment\nsocks://127.0.0.1:1080") ==
          QList<QString>{"# { comment", "// } comment", "socks://127.0.0.1:1080"},
          "non-JSON lines and comments retain exact splitter text");
    check(Subscription::JsonEndIdx("{}{}", 0) == 1 && Subscription::JsonEndIdx("{}{}", 2) == 3,
          "empty adjacent object boundaries unchanged");
    // Missing closes and unterminated strings yield no complete object.
    const QList<QString> incomplete{
        "{", "{\"tag\":\"{\"", "{\"nested\":{}", "{\"tag\":\"unterminated}",
        "{\"tag\":\"trailing\\", "{\"tag\":\"escaped\\\"}"
    };
    for (const auto &bad : incomplete) {
        check(Subscription::JsonEndIdx(bad, 0) == -1, "incomplete object does not acquire a false end");
        check(trimmedParts(first + '\n' + bad) == QList<QString>{first},
              "completed prefix retained while incomplete tail is dropped");
        Subscription::RawUpdater updater;
        updater.update(outbound("first") + '\n' + bad);
        check(updater.proxies.size() == 1 && updater.proxies[0]->storage->config_simple == outbound("first"),
              "import rejects incomplete tail without losing completed prefix");
    }
    // The splitter finds structure only; Qt continues to reject bad syntax.
    // Use single-line invalid objects so the pre-existing malformed-multiline
    // retry behavior in RawUpdater is not entered by this focused test.
    const QList<QString> invalid{
        "{\"type\":\"socks\",}", "{\"type\":\"socks\" \"tag\":1}",
        "{\"type\":\"socks\",\"extra\":[1,]}", "{\"type\":\"socks\",\"tag\":\"bad\\uZZZZ\"}"
    };
    for (const auto &bad : invalid) {
        check(Subscription::JsonEndIdx(bad, 0) == bad.size() - 1,
              "balanced invalid syntax is still deferred to JSON parser");
        QJsonParseError error;
        QJsonDocument::fromJson(bad.toUtf8(), &error);
        check(error.error != QJsonParseError::NoError, "Qt still rejects malformed JSON syntax");
        Subscription::RawUpdater updater;
        updater.update(bad + '\n' + outbound("second"));
        check(updater.proxies.size() == 1 && updater.proxies[0]->storage->config_simple == outbound("second"),
              "invalid single-line object rejected and later valid object imported");
    }
    const QString link = "socks://127.0.0.1:1080";
    for (const auto &wrapped : {link + '\n', "\n " + link + " \r\n", link + "\n\n"}) {
        const QList<QString> inputs{
            wrapped,
            QString::fromUtf8(QJsonDocument(QJsonArray{wrapped}).toJson(QJsonDocument::Compact)),
            QString::fromUtf8(QJsonDocument(QJsonArray{QJsonObject{{"proxy", wrapped}}}).toJson(QJsonDocument::Compact))
        };
        for (const auto &input : inputs) {
            Subscription::RawUpdater updater;
            updater.update(input);
            check(updater.proxies.size() == 1 && updater.proxies[0]->type == "socks",
                  "no-progress guard still permits share-link whitespace normalization");
        }
    }
    // Envelope/full-config dispatch receives complete JSON too. The test
    // boundaries capture dispatch and text, not protocol/sanitizer internals.
    {
        for (const auto &kind : {QString("outbounds"), QString("endpoints"), QString("full")}) {
            const QJsonObject item = QJsonDocument::fromJson(outbound("}{\\\"").toUtf8()).object();
            QJsonObject obj{{kind == "endpoints" ? "endpoints" : "outbounds", QJsonArray{item}}, {"remarks", "fixture }{"}};
            if (kind == "full") obj["dns"] = QJsonObject();
            const auto text = QString::fromUtf8(QJsonDocument(obj).toJson(
                QJsonDocument::Compact)).trimmed();
            Subscription::RawUpdater updater;
            updater.update(text + '\n' + outbound("last"));
            if (kind == "full") {
                check(updater.proxies.size() == 2 && updater.envelopes.empty(),
                      "multiline full configuration retains full-config dispatcher");
                if (updater.proxies.size() == 2) check(updater.proxies[0]->storage->core == "internal-full",
                                                      "full configuration retains full mode");
            } else {
                check(updater.envelopes == QList<QJsonObject>{obj} && updater.proxies.size() == 1,
                      "multiline outbound/endpoint envelope reaches dispatcher intact");
            }
            check(!updater.proxies.empty() && updater.proxies.last()->storage->config_simple == outbound("last"),
                  "entry after envelope/full configuration survives import");
        }
    }
    std::cout << "RESULT: " << total - failures << "/" << total
              << " checks passed; " << failures << " failed\n";
    return failures ? 1 : 0;
}
