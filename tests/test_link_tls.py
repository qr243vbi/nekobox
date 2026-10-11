#!/usr/bin/env python3
"""Compile verbatim TLS share-query import/export helpers and their regressions.

Run: python3 tests/test_link_tls.py [--repo PATH] [--ref COMMIT]
Default mode requires Qt6 Core development files and pkg-config. Explicit
--mode ascii-adapter needs only Python 3 and a GCC/Clang-compatible C++20 compiler
(CXX, or c++); it tests helper logic with ASCII container/string stand-ins, not
Qt parsing. Neither mode builds the application, opens profiles, or uses a VPN.
The six caller checks are source-routing checks, not full protocol import tests.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
PROXY = 'src/gharqad/configs/proxy/'
CALLERS = ('HttpBean', 'AnyTLSBean', 'ShadowTLSBean', 'NaiveBean',
           'JuicityBean', 'TrustTunnelBean')


def section(source, start, end):
    if source.count(start) != 1 or source.count(end) != 1:
        raise ValueError(f'Source anchors changed: {start!r}, {end!r}')
    return source.split(start, 1)[1].split(end, 1)[0]


def function(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f'Expected one function: {signature}')
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for pos in range(brace, len(source)):
        depth += (source[pos] == '{') - (source[pos] == '}')
        if depth == 0:
            return source[start:pos + 1]
    raise ValueError(f'Unterminated function: {signature}')


SUPPORT = r'''
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#ifdef USE_REAL_QT
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#else
// Deliberately ASCII-only boundaries; these do not reproduce Qt URL parsing.
struct QString : std::string {
    using std::string::string;
    QString(const std::string &value) : std::string(value) {}
    bool isEmpty() const { return empty(); }
    QString trimmed() const {
        auto first = find_first_not_of(" \t\n\r");
        return first == npos ? "" : substr(first, find_last_not_of(" \t\n\r") - first + 1);
    }
};
struct QStringList : std::vector<QString> {
    using std::vector<QString>::vector;
    bool contains(const QString &value) const {
        return std::find(begin(), end(), value) != end();
    }
};
class QUrlQuery {
    std::vector<std::pair<QString, QString>> items;
public:
    void addQueryItem(const QString &name, const QString &value) { items.emplace_back(name, value); }
    QString queryItemValue(const QString &name) const {
        for (const auto &[key, value] : items) if (key == name) return value;
        return "";
    }
    bool hasQueryItem(const QString &name) const {
        for (const auto &[key, value] : items) { (void)value; if (key == name) return true; }
        return false;
    }
};
#endif
// Unrelated enum/storage dependencies are stand-ins in both modes. Actual
// V2rayStreamSettings field declarations/defaults are inserted by the runner.
struct V2RAYTransportsEnum : QString { using QString::QString; };
struct VmessPacketEncodingsEnum { explicit VmessPacketEncodingsEnum(int) {} };
struct KCPExtra {};
#define DECLARE_STORE_TYPE(...)
QString GetQueryValue(const QUrlQuery &, const QString &, const QString & = "");
'''

CHECKS = r'''
int total = 0, failed = 0;
void check(bool passed, const char *label) {
    ++total;
    if (!passed) { ++failed; std::cout << "FAIL " << label << '\n'; }
}
int main() {
    using Configs::V2rayStreamSettings;
    check(!V2rayStreamSettings().allow_insecure, "production secure default");
    struct Input { const char *text; bool expected; };
    const Input inputs[] = {
        {nullptr, false}, {"", false}, {"true", true}, {"1", true},
        {"false", false}, {"0", false}, {"invalid", false}, {"TRUE", false},
        {"FALSE", false}, {"True", false}, {" true", false}, {"1 ", false}
    };
    for (const auto &input : inputs) {
        for (bool initial : {false, true}) {
            QUrlQuery query;
            if (input.text) query.addQueryItem("insecure", input.text);
            auto stream = std::make_shared<V2rayStreamSettings>();
            stream->allow_insecure = initial;
            Configs::From_Link::add_tls(stream, query);
            if (stream->allow_insecure != input.expected)
                std::cout << "input=" << (input.text ? input.text : "<missing>")
                          << " initial=" << initial << '\n';
            check(stream->allow_insecure == input.expected, "insecure query value");
            check(stream->security == "tls", "TLS still enabled");
        }
    }
    for (bool reality : {false, true}) {
        for (bool value : {false, true}) {
            for (bool initial : {false, true}) {
                auto original = std::make_shared<V2rayStreamSettings>();
                original->security = "tls";
                original->allow_insecure = value;
                if (reality) original->reality_pbk = "synthetic-public-key";
                QUrlQuery query;
                Configs::To_Link::add_tls(original, query);
                check(query.hasQueryItem("insecure") == value, "export omits insecure=false");
                if (value) check(query.queryItemValue("insecure") == "1", "export uses insecure=1");
#ifdef USE_REAL_QT
                // Real Qt URL serialization/reparse, with no network activity.
                QUrl url("https://example.invalid:443");
                url.setQuery(query);
                QUrl importedUrl(url.toString(QUrl::FullyEncoded));
                query = QUrlQuery(importedUrl.query(QUrl::FullyDecoded));
#endif
                auto imported = std::make_shared<V2rayStreamSettings>();
                imported->allow_insecure = initial;
                Configs::From_Link::add_tls(imported, query);
                check(imported->allow_insecure == value, "TLS export/import roundtrip");
                check(imported->reality_pbk == original->reality_pbk, "reality key unchanged");
            }
        }
    }
    // Other fields in the shared importer/exporter retain their behavior.
    auto original = std::make_shared<V2rayStreamSettings>();
    original->security = "tls";
    original->sni = "sni.invalid";
    original->alpn = "h2,http/1.1";
    original->enable_ech = true;
    original->ech_config = "synthetic-ech-text";
    original->query_server_name = "query.invalid";
    original->utlsFingerprint = "chrome";
    original->enable_tls_fragment = true;
    original->tls_fragment_fallback_delay = "100ms";
    original->enable_tls_record_fragment = true;
    original->reality_pbk = "synthetic-public-key";
    original->reality_sid = "synthetic-short-id";
    QUrlQuery query;
    Configs::To_Link::add_tls(original, query);
    auto imported = std::make_shared<V2rayStreamSettings>();
    Configs::From_Link::add_tls(imported, query);
#define SAME(field) check(imported->field == original->field, #field " unchanged")
    SAME(sni); SAME(alpn); SAME(enable_ech); SAME(ech_config);
    SAME(query_server_name); SAME(utlsFingerprint); SAME(enable_tls_fragment);
    SAME(tls_fragment_fallback_delay); SAME(enable_tls_record_fragment);
    SAME(reality_pbk); SAME(reality_sid);
#undef SAME
    query.addQueryItem("peer", "peer.invalid");
    Configs::From_Link::add_tls(imported, query);
    check(imported->sni == "peer.invalid", "peer retains SNI precedence");
    QUrlQuery empty;
    Configs::From_Link::add_tls(imported, empty);
    check(!imported->enable_ech, "absent ECH disabled");
    check(imported->utlsFingerprint == Configs::dataStore->utlsFingerprint, "default fingerprint retained");
    std::cout << "SUMMARY checks=" << total << " failures=" << failed << '\n';
    return failed ? 1 : 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parent)
    parser.add_argument('--ref', help='Read immutable Git revision instead of working files')
    parser.add_argument('--mode', choices=('qt6', 'ascii-adapter'), default='qt6')
    args = parser.parse_args()
    repo = args.repo.resolve()

    def read(path):
        if args.ref:
            return subprocess.check_output(['git', '-C', str(repo), 'show', f'{args.ref}:{path}'], text=True)
        return (repo / path).read_text(encoding='utf-8')

    flags = []
    if args.mode == 'qt6':
        probe = subprocess.run(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True, capture_output=True)
        if probe.returncode:
            raise SystemExit('BLOCKED: Qt6 development files unavailable; --mode ascii-adapter is limited helper validation.')
        flags = ['-DUSE_REAL_QT', *shlex.split(probe.stdout)]
        if sys.platform.startswith('linux'):
            flags.append('-fPIC')
    for caller in CALLERS:
        body = function(read(PROXY + caller + '.cpp'), f'bool {caller}::TryParseLink(')
        if body.count('add_tls(stream, query)') != 1 or 'allow_insecure' in body:
            raise ValueError(f'{caller} TLS routing changed; review test coverage')
    print('SOURCE routing checks: six importers use the shared TLS helper (not runtime integration)', flush=True)
    stream = read('src/nekobox/configs/proxy/V2RayStreamSettings.hpp')
    fields = section(stream, 'class V2rayStreamSettings : public JsonStore {',
                     'V2rayStreamSettings() : JsonStore()')
    parse = read(PROXY + 'Link2Bean.cpp')
    export = read(PROXY + 'Bean2Link.cpp')
    utils = read('src/gharqad/dataStore/Utils.cpp')
    code = SUPPORT + function(utils, 'QString GetQueryValue(') + '\n'
    code += function(utils, 'void AddQueryString(') + '\nnamespace Configs {\n'
    code += 'struct V2rayStreamSettings {' + fields + '\n};\n'
    code += 'struct DataStore { QString utlsFingerprint = "firefox"; };\nDataStore defaults;\nDataStore *dataStore = &defaults;\n'
    code += 'namespace From_Link { void add_tls(std::shared_ptr<V2rayStreamSettings>, QUrlQuery &); }\n'
    code += function(parse, 'void From_Link::add_tls(') + '\nnamespace To_Link {\n'
    code += function(export, 'void add_query_nonempty(') + '\n'
    code += function(export, 'void add_tls(') + '\n}\n}\n' + CHECKS
    print('SOURCE:', args.ref or 'working tree', '| importer SHA256:', hashlib.sha256(parse.encode()).hexdigest(), flush=True)
    print('MODE:', args.mode, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-link-tls-') as temp:
        cpp = Path(temp) / 'regression.cpp'
        binary = Path(temp) / ('regression.exe' if os.name == 'nt' else 'regression')
        cpp.write_text(code, encoding='utf-8')
        command = shlex.split(os.environ.get('CXX', 'c++'))
        command += ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', str(cpp), '-o', str(binary), *flags]
        subprocess.run(command, check=True, timeout=60)
        return subprocess.run([str(binary)], timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
