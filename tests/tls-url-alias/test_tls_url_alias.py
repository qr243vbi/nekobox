#!/usr/bin/env python3
"""Compile source-linked TLS URL alias parsing, export/reimport and JSON generation.

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
PRIMARY = 'query.queryItemValue("allowInsecure")'
SELECT = 'query.queryItemValue(query.hasQueryItem("allowInsecure") ? "allowInsecure" : "insecure")'
GATE = '(query.hasQueryItem("allowInsecure") || QUrl(link, QUrl::StrictMode).isValid())'
MUTATIONS = ('no-fallback', 'alias-first', 'empty-fallback', 'or-alias', 'nonempty',
             'always-false', 'preserve-stale', 'last-duplicate', 'casefold', 'trim',
             'omit-gate', 'normalized-url-gate', 'gate-primary-too', 'tolerant-gate', 'query-only-gate',
             'secure-on-conflict', 'decode-before-split', 'omit-json-flag', 'omit-export-flag')


def mutate(path, source, name):
    before = after = None
    if path in (PROXY + 'VMessBean.cpp', PROXY + 'TrojanVLESSBean.cpp'):
        before = SELECT
        replacements = {
            'no-fallback': PRIMARY,
            'alias-first': 'query.queryItemValue(query.hasQueryItem("insecure") ? "insecure" : "allowInsecure")',
            'empty-fallback': 'query.queryItemValue(query.queryItemValue("allowInsecure").isEmpty() ? "insecure" : "allowInsecure")',
            'casefold': SELECT + '.toLower()',
            'trim': SELECT + '.trimmed()',
            'last-duplicate': 'query.allQueryItemValues(query.hasQueryItem("allowInsecure") ? "allowInsecure" : "insecure").value(query.allQueryItemValues(query.hasQueryItem("allowInsecure") ? "allowInsecure" : "insecure").size() - 1)',
        }
        after = replacements.get(name)
        if name in ('nonempty', 'always-false', 'preserve-stale', 'secure-on-conflict', 'or-alias'):
            match = re.search(re.escape('QStringList{"1", "true"}.contains(') + r'\s*' + re.escape(SELECT + ')'), source)
            if not match:
                raise ValueError('Mutation expression changed: ' + name)
            before = match.group()
            after = {'nonempty': '!' + SELECT + '.isEmpty()',
                     'always-false': 'false',
                     'preserve-stale': '(stream->allow_insecure || ' + before + ')',
                     'or-alias': '(' + before + ' || QStringList{"1", "true"}.contains(query.queryItemValue("insecure")))',
                     'secure-on-conflict': before + ' && (!query.hasQueryItem("insecure") || QStringList{"1", "true"}.contains(query.queryItemValue("insecure")))'}[name]
        if name in ('omit-gate', 'normalized-url-gate', 'gate-primary-too', 'tolerant-gate', 'query-only-gate'):
            before = GATE
            after = {
                'omit-gate': 'true',
                'normalized-url-gate': GATE.replace('QUrl(link,', 'QUrl(url.toString(QUrl::FullyEncoded),'),
                'gate-primary-too': 'QUrl(link, QUrl::StrictMode).isValid()',
                'tolerant-gate': GATE.replace('QUrl::StrictMode', 'QUrl::TolerantMode'),
                'query-only-gate': '(query.hasQueryItem("allowInsecure") || QUrl("https://example.invalid/?" + url.query(QUrl::FullyEncoded), QUrl::StrictMode).isValid())',
            }[name]
    elif path == 'src/nekobox/dataStore/Utils.hpp' and name == 'decode-before-split':
        before, after = '#define GetQuery(url) QUrlQuery(url)', '#define GetQuery(url) QUrlQuery(url.query(QUrl::FullyDecoded))'
    elif path == PROXY + 'Bean2CoreObj_box.cpp' and name == 'omit-json-flag':
        before = 'if (allow_insecure || Configs::dataStore->skip_cert) tls["insecure"] = true;'
        after = 'if (Configs::dataStore->skip_cert) tls["insecure"] = true;'
    elif path == PROXY + 'Bean2Link.cpp' and name == 'omit-export-flag':
        before = 'if (stream->allow_insecure) query.addQueryItem("insecure", "1");'
        after = '/* mutation: omit export flag */'
    if after is not None:
        if source.count(before) != 1:
            raise ValueError('Mutation anchor changed: ' + name + ' in ' + path)
        source = source.replace(before, after)
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    parser.add_argument('--mutation', choices=MUTATIONS)
    parser.add_argument('--mutation-target', choices=('both', 'trojan-vless', 'vmess'), default='both')
    parser.add_argument('--keep-generated', type=Path)
    parser.add_argument('--primary-only', action='store_true', help='Only assert primary-key behavior for differential prerequisite comparison')
    args = parser.parse_args()
    hashes = {}

    def read(path):
        source = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
        targets = {'both': None, 'trojan-vless': PROXY + 'TrojanVLESSBean.cpp', 'vmess': PROXY + 'VMessBean.cpp'}
        if args.mutation and (targets[args.mutation_target] is None or path == targets[args.mutation_target]):
            source = mutate(path, source, args.mutation)
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
    for bean in ('TrojanVLESS', 'VMess', 'AnyTLS'):
        name = bean + 'Bean'
        header = read(HEADER + name + '.hpp')
        ctor_sig = name + '(Configs::ProxyEntity * entity'
        ctor = function(header, ctor_sig)
        start = header.index('class ' + name + ' : public AbstractBean {')
        prefix = re.sub(r'\bexplicit\s*$', '', header[start:header.index(ctor_sig, start)])
        code += prefix + ctor + '\n'
        code += function(header, 'virtual QString type()const override') + '\n'
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
    for bean in ('TrojanVLESS', 'VMess', 'AnyTLS'):
        source = read(PROXY + bean + 'Bean.cpp')
        for signature in ('bool ' + bean + 'Bean::TryParseLink(', 'QString ' + bean + 'Bean::ToShareLink() const', 'CoreObjOutboundBuildResult ' + bean + 'Bean::BuildCoreObjSingBox() const'):
            code += function(source, signature) + '\n'
    code += '}\n' + (HERE / 'checks.cpp').read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True)
    print('SOURCE:', args.ref or 'working tree', '| mutation:', args.mutation or 'none', '| Qt:', subprocess.check_output(['pkg-config', '--modversion', 'Qt6Core'], text=True).strip(), flush=True)
    for path, digest in hashes.items():
        print(digest, path, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-tls-url-alias-') as temp:
        cpp = Path(temp) / 'test.cpp'
        exe = Path(temp) / 'test'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC']
        if args.primary_only:
            command += ['-DPRIMARY_ONLY']
        command += shlex.split(os.environ.get('EXTRA_CXXFLAGS', '')) + [str(cpp), '-o', str(exe)] + shlex.split(flags)
        subprocess.run(command, check=True, timeout=90)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
