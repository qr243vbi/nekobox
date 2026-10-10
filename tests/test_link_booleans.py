#!/usr/bin/env python3
"""Compile the actual boolean import/export helper bodies and exercise regressions.

Requires Python 3 and g++; default mode also requires pkg-config and Qt6Core
development files. This runner does not configure a Windows/MSVC toolchain.
Explicit runtime-adapter mode is Linux-specific and
uses a small QString/QUrlQuery adapter and calls the installed Qt6 runtime's
localeAwareCompare_helper. That mode validates the actual C++ helper branch
logic, not full QUrl parsing, widgets, profiles, or the application build.
"""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent

def extract_function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

ADAPTER = r'''
#include <dlfcn.h>
#include <map>
#include <stdexcept>
#include <string>
// Installed Qt6Core exports this exact signature on this Linux cloud executor.
// No replacement comparison implementation is used.
using Compare = int (*)(const char16_t *, long long, const char16_t *, long long);
Compare compare_function() {
    static auto function = [] {
        void *library = dlopen("libQt6Core.so.6", RTLD_NOW | RTLD_LOCAL);
        if (!library) throw std::runtime_error(dlerror());
        auto f = reinterpret_cast<Compare>(dlsym(library,
            "_ZN7QString25localeAwareCompare_helperEPK5QCharxS2_x"));
        if (!f) throw std::runtime_error(dlerror());
        return f;
    }();
    return function;
}
class QString {
public:
    std::u16string data;
    QString(const char *value = "") {
        while (*value) data += static_cast<unsigned char>(*value++);
    }
    int localeAwareCompare(const char *other) const {
        const QString rhs(other);
        return compare_function()(data.data(), data.size(), rhs.data.data(), rhs.data.size());
    }
};
class QUrlQuery {
    std::map<std::string, QString> items;
public:
    void addQueryItem(const char *name, const QString &value) { items.emplace(name, value); }
    QString queryItemValue(const char *name) const {
        auto found = items.find(name);
        return found == items.end() ? QString() : found->second;
    }
};
'''

TESTS = r'''
#include <array>
#include <iostream>
struct Field { const char *name; bool initial; };
const std::array<Field, 11> fields = {{
    {"use_system_interface", false}, {"enable_amnezia", false},
    {"ephemeral", false}, {"accept_routes", false},
    {"exit_node_allow_lan_access", false}, {"advertise_exit_node", false},
    {"globalDNS", false}, {"global_dns", false},
    {"global_padding", true}, {"authenticated_length", false},
    {"health_check", true}
}};
int total = 0, failed = 0;
void check(const char *category, const char *key, const char *input,
           bool initial, bool got, bool expected) {
    ++total;
    if (got == expected) return;
    ++failed;
    if (failed <= 18)
        std::cout << "FAIL " << category << " " << key << "="
                  << (input ? input : "<missing>") << " initial=" << initial
                  << " expected=" << expected << " actual=" << got << '\n';
}
int main() {
    // All current callers, both initial values, canonical values and fallback cases.
    const char *inputs[] = {nullptr, "true", "false", "", "invalid", "TRUE", "FALSE", "0", "1"};
    for (const auto &field : fields) {
        for (bool initial : {false, true}) {
            for (const char *input : inputs) {
                QUrlQuery query;
                if (input) query.addQueryItem(field.name, input);
                bool value = initial;
                Configs::From_Link::set_boolean(field.name, value, query);
                const bool expected = input && std::string(input) == "true" ? true :
                                      input && std::string(input) == "false" ? false : initial;
                check("parse", field.name, input, initial, value, expected);
            }
        }
        // Exercise the actual exporter body and importer body together.
        for (bool exported : {false, true}) {
            for (bool initial : {false, true}) {
                QUrlQuery query;
                Configs::To_Link::add_query_boolean(field.name, query, exported);
                bool value = initial;
                Configs::From_Link::set_boolean(field.name, value, query);
                check("roundtrip", field.name, exported ? "true" : "false", initial, value, exported);
            }
        }
    }
    // Tailscale calls the same helper twice, first for the legacy key then current.
    for (const char *legacy : {static_cast<const char *>(nullptr), "true", "false"}) {
        for (const char *current : {static_cast<const char *>(nullptr), "true", "false"}) {
            QUrlQuery query;
            if (legacy) query.addQueryItem("globalDNS", legacy);
            if (current) query.addQueryItem("global_dns", current);
            bool value = false;
            Configs::From_Link::set_boolean("globalDNS", value, query);
            Configs::From_Link::set_boolean("global_dns", value, query);
            const char *selected = current ? current : legacy;
            const bool expected = selected && std::string(selected) == "true";
            check("alias", "global_dns", selected, false, value, expected);
        }
    }
    std::cout << (failed ? "FAIL" : "PASS") << ": " << (total - failed) << "/"
              << total << " checks passed; " << failed << " failed\n";
    return failed ? 1 : 0;
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=HERE.parent / 'src/gharqad/configs/proxy/Link2Bean.cpp')
    parser.add_argument('--export-source', type=Path, default=HERE.parent / 'src/gharqad/configs/proxy/Bean2Link.cpp')
    parser.add_argument('--mode', choices=['qt6', 'runtime-adapter'], default='qt6')
    args = parser.parse_args()
    source = args.source.read_text()
    parse = extract_function(source, 'void From_Link::set_boolean(')
    export = extract_function(args.export_source.read_text(), 'void add_query_boolean(')
    if args.mode == 'qt6':
        probe = subprocess.run(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True, capture_output=True)
        if probe.returncode:
            raise SystemExit('BLOCKED: Qt6 development package unavailable. Use explicit --mode runtime-adapter for limited isolated validation.')
        headers = '#include <QString>\n#include <QUrlQuery>\n#include <string>\n'
        flags = shlex.split(probe.stdout)
    else:
        headers = ADAPTER
        flags = ['-ldl']
    harness = headers + r'''
namespace Configs {
namespace From_Link {
void set_boolean(const char *name, bool &value, const QUrlQuery &obj);
}
''' + parse + r'''
namespace To_Link {
void add_query_nonempty(const char *name, QUrlQuery &query, const QString &value) {
    query.addQueryItem(name, value);
}
''' + export + '\n}\n}\n' + TESTS
    print('Mode:', args.mode, '| Source:', args.source, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-275-') as td:
        cpp = Path(td) / 'test.cpp'
        binary = Path(td) / 'test'
        cpp.write_text(harness)
        compile_flags = ['-std=c++20', '-Wall', '-Wextra', '-Werror']
        if sys.platform.startswith('linux'):
            compile_flags.append('-fPIC')  # Required by Qt builds with reduced relocations.
        subprocess.run(['g++', *compile_flags, str(cpp), '-o', str(binary), *flags], check=True)
        return subprocess.run([str(binary)]).returncode

if __name__ == '__main__':
    raise SystemExit(main())
