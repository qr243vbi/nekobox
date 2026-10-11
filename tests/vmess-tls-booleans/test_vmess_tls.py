#!/usr/bin/env python3
"""Compile source-linked VMess TLS URL/Base64 parsing, sharing and JSON generation.

Real Qt6Core; synthetic in-memory fixtures only. No application/core/network run.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from test_link_tls import function, section

PROXY = 'src/gharqad/configs/proxy/'
HEADER = 'src/nekobox/configs/proxy/'
FIXED = ('stream->allow_insecure =\n'
         '                QStringList{"1", "true"}.contains(query.queryItemValue("allowInsecure"));')
OLD = 'if (!query.queryItemValue("allowInsecure").isEmpty()) stream->allow_insecure = true;'
MUTATIONS = {
    'nonempty': (PROXY + 'VMessBean.cpp', FIXED, OLD),
    'always-false': (PROXY + 'VMessBean.cpp', FIXED, 'stream->allow_insecure = false;'),
    'preserve-stale': (PROXY + 'VMessBean.cpp', FIXED,
                       'if (QStringList{"1", "true"}.contains(query.queryItemValue("allowInsecure"))) stream->allow_insecure = true;'),
    'one-only': (PROXY + 'VMessBean.cpp', FIXED,
                 'stream->allow_insecure = query.queryItemValue("allowInsecure") == "1";'),
    'case-insensitive': (PROXY + 'VMessBean.cpp', FIXED,
                         'stream->allow_insecure = QStringList{"1", "true"}.contains(query.queryItemValue("allowInsecure").toLower());'),
    'trimmed': (PROXY + 'VMessBean.cpp', FIXED,
                'stream->allow_insecure = QStringList{"1", "true"}.contains(query.queryItemValue("allowInsecure").trimmed());'),
    'decode-before-split': ('src/nekobox/dataStore/Utils.hpp', '#define GetQuery(url) QUrlQuery(url)',
                            '#define GetQuery(url) QUrlQuery(url.query(QUrl::FullyDecoded))'),
    'omit-json-flag': (PROXY + 'Bean2CoreObj_box.cpp',
                       'if (allow_insecure || Configs::dataStore->skip_cert) tls["insecure"] = true;',
                       'if (Configs::dataStore->skip_cert) tls["insecure"] = true;'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    parser.add_argument('--mutation', choices=MUTATIONS)
    parser.add_argument('--keep-generated', type=Path)
    parser.add_argument('--native-json-probe', action='store_true')
    parser.add_argument('--expect-native-json-true', action='store_true', help='Additional assertion for combined compatibility with the independent Node bool fix')
    args = parser.parse_args()
    if args.expect_native_json_true and not args.native_json_probe:
        parser.error('--expect-native-json-true requires --native-json-probe')
    hashes = {}

    def read(path):
        source = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
        if args.mutation:
            target, before, after = MUTATIONS[args.mutation]
            if path == target:
                if source.count(before) != 1 and before == FIXED:
                    # The later URL-alias compatibility change keeps this
                    # strict rule but supplies a presence-selected key.
                    primary = 'query.queryItemValue("allowInsecure")'
                    selected = 'query.queryItemValue(query.hasQueryItem("allowInsecure") ? "allowInsecure" : "insecure")'
                    gate = '(query.hasQueryItem("allowInsecure") || QUrl(link, QUrl::StrictMode).isValid())'
                    pattern = (r'stream->allow_insecure\s*=\s*' +
                               '(?:' + re.escape(gate) + r'\s*&&\s*)?' +
                               re.escape('QStringList{"1", "true"}.contains(') +
                               r'\s*' + re.escape(selected + ');'))
                    matches = re.findall(pattern, source)
                    if len(matches) == 1:
                        before = matches[0]
                        after = after.replace(primary, selected)
                        if gate in before:
                            after = f'if ({gate}) {{ {after} }} else {{ stream->allow_insecure = false; }}'
                if source.count(before) != 1:
                    raise ValueError('Mutation anchor changed: ' + args.mutation)
                source = source.replace(before, after)
        hashes[path] = hashlib.sha256(source.encode()).hexdigest()
        return source

    utils = read('src/gharqad/dataStore/Utils.cpp')
    utils_h = read('src/nekobox/dataStore/Utils.hpp')
    stream = read(HEADER + 'V2RayStreamSettings.hpp')
    extra = read(HEADER + 'AbstractBeanExtra.hpp')
    parse = read(PROXY + 'Link2Bean.cpp')
    export = read(PROXY + 'Bean2Link.cpp')
    builder = read(PROXY + 'Bean2CoreObj_box.cpp')
    fields = section(stream, 'class V2rayStreamSettings : public JsonStore {', 'V2rayStreamSettings() : JsonStore()')
    code = ''
    if args.native_json_probe:
        import native_json
        code += native_json.generate(read)
    code += (HERE / 'support.hpp').read_text().replace('STREAM_FIELDS', fields)
    start = utils_h.index('#define GetQuery(url)')
    code += '\n' + utils_h[start:utils_h.index('\n\n', start)] + '\n'
    code += '\nnamespace QtBase64 {' + section(utils_h, 'namespace QtBase64 {', 'namespace Qt515Base64 = QtBase64;') + '\nnamespace Qt515Base64 = QtBase64;\n'
    code += read('src/gharqad/dataStore/base64.cpp').replace('#include <nekobox/dataStore/Utils.hpp>', '')
    code += function(read('src/3rdparty/qv2ray/wrapper.hpp'), 'inline QJsonObject QString2QJsonObject(') + '\n'
    for signature in ('QByteArray DecodeB64IfValid(', 'QString GetQueryValue(', 'int GetQueryIntValue(', 'void AddQueryString(', 'void AddQueryInt(', 'void AddQueryNatural(',
                      'QString SubStrBefore(', 'QString SubStrAfter(',
                      'QJsonArray QListStr2QJsonArray(', 'QJsonObject QMapString2QJsonObject('):
        code += function(utils, signature) + '\n'
    code += '\nnamespace Configs {\n'
    for bean in ('VMess',):
        name = bean + 'Bean'
        header = read(HEADER + name + '.hpp')
        ctor_sig = name + '(Configs::ProxyEntity * entity'
        ctor = function(header, ctor_sig)
        start = header.index('class ' + name + ' : public AbstractBean {')
        prefix = re.sub(r'\bexplicit\s*$', '', header[start:header.index(ctor_sig, start)])
        code += prefix + ctor + '\n'
        code += function(header, 'virtual QString type()const override') + '\n'
        if args.native_json_probe:
            code += 'bool TryParseJson(const Data::Node &);\n'
        code += 'bool TryParseLink(const QString &); QString ToShareLink() const; CoreObjOutboundBuildResult BuildCoreObjSingBox() const; };\n'
    code += 'namespace From_Link {\ntemplate<typename T> ' + function(extra, 'void add_network(T * t, const QUrlQuery &obj)') + '\n}\n'
    for signature in ('void From_Link::set_boolean(', 'void From_Link::add_tls(', 'void From_Link::add_default_fields(', 'void From_Link::add_mux_state('):
        code += function(parse, signature) + '\n'
    code += 'namespace To_Link {\ntemplate<typename T> ' + function(extra, 'void add_network(QUrlQuery & query, T * obj)') + '\n'
    for signature in ('void add_query_nonempty(', 'void add_query_int_natural(', 'void add_default_fields(', 'void add_query_boolean(', 'void add_mux_state(', 'void add_tls('):
        code += function(export, signature) + '\n'
    code += '}\nnamespace To_CoreObj_box {\n'
    for signature, template in (('inline void add_default_fields(T & obj, const AbstractBean * bean)', 'typename T'),
                                ('inline void add_non_empty(T & obj, const QString & key, const QString & value)', 'typename T'),
                                ('inline void add_network(T & obj, B * bean)', 'typename T, typename B')):
        code += 'template<' + template + '> ' + function(extra, signature) + '\n'
    code += '}\nusing namespace To_CoreObj_box;\n'
    code += function(stream, 'QMap<QString, QString> GetHeaderPairs(bool* ok)').replace('GetHeaderPairs(', 'V2rayStreamSettings::GetHeaderPairs(', 1) + '\n'
    code += function(builder, 'void V2rayStreamSettings::BuildStreamSettingsSingBox(') + '\n'
    for bean in ('VMess',):
        source = read(PROXY + bean + 'Bean.cpp')
        for signature in ('bool ' + bean + 'Bean::TryParseLink(', 'QString ' + bean + 'Bean::ToShareLink() const', 'CoreObjOutboundBuildResult ' + bean + 'Bean::BuildCoreObjSingBox() const'):
            code += function(source, signature) + '\n'
    if args.native_json_probe:
        code += native_json.imports(read)
    code += '}\n' + (HERE / ('native_json_checks.cpp' if args.native_json_probe else 'checks.cpp')).read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True)
    print('SOURCE:', args.ref or 'working tree', '| mutation:', args.mutation or 'none', '| Qt:', subprocess.check_output(['pkg-config', '--modversion', 'Qt6Core'], text=True).strip(), flush=True)
    for path, digest in hashes.items():
        print(digest, path, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-vmess-tls-') as temp:
        cpp = Path(temp) / 'test.cpp'
        exe = Path(temp) / 'test'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC']
        if args.native_json_probe:
            command += ['-Wno-deprecated-copy']
        if args.expect_native_json_true:
            command += ['-DEXPECT_NATIVE_JSON_TRUE']
        command += shlex.split(os.environ.get('EXTRA_CXXFLAGS', '')) + [str(cpp), '-o', str(exe)] + shlex.split(flags)
        subprocess.run(command, check=True, timeout=90)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
