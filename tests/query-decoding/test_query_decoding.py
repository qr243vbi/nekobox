#!/usr/bin/env python3
"""Compile verbatim URL parser/exporter functions against real Qt6Core.

Synthetic fixtures only. Does not build/start the application or core.
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

BEANS = ('AnyTLS', 'Http', 'Juicity', 'Mieru', 'Naive', 'SSH', 'ShadowSocks',
         'ShadowTLS', 'Socks', 'Tailscale', 'Tor', 'TrojanVLESS', 'TrustTunnel',
         'VMess', 'Wireguard')
PROXY = 'src/gharqad/configs/proxy/'
HEADER = 'src/nekobox/configs/proxy/'
MUTATIONS = {
    'decode-before-split': ('src/nekobox/dataStore/Utils.hpp', '#define GetQuery(url) QUrlQuery(url)', '#define GetQuery(url) QUrlQuery(url.query(QUrl::FullyDecoded))'),
    'pretty-getter': ('src/gharqad/dataStore/Utils.cpp', 'q.queryItemValue(key, QUrl::FullyDecoded)', 'q.queryItemValue(key)'),
    'unprotected-percent': ('src/gharqad/dataStore/Utils.cpp', 'QString(value).replace("%", "%25")', 'value'),
    'tailscale-double-decode': (PROXY + 'TailscaleBean.cpp', 'auth_key = GetQueryValue(query, "auth_key");', 'auth_key = QUrl::fromPercentEncoding(GetQueryValue(query, "auth_key").toUtf8());'),
    'tailscale-preencoded-export': (PROXY + 'TailscaleBean.cpp', 'add_query_nonempty("auth_key", q, auth_key);', 'add_query_nonempty("auth_key", q, QUrl::toPercentEncoding(auth_key));'),
    'ssh-pretty-getter': (PROXY + 'SSHBean.cpp', 'password = GetQueryValue(query, "password");', 'password = query.queryItemValue("password");'),
    'wireguard-pretty-getter': (PROXY + 'WireguardBean.cpp', 'privateKey = GetQueryValue(query, "private_key");', 'privateKey = query.queryItemValue("private_key");'),
    'shadowsocks-pretty-getter': (PROXY + 'ShadowSocksBean.cpp', 'GetQueryValue(query, "plugin")', 'query.queryItemValue("plugin")'),
    'mieru-pretty-getter': (PROXY + 'MieruBean.cpp', 'GetQueryValue(query, "multiplexing")', 'query.queryItemValue("multiplexing")'),
    'tls-delay-pretty-getter': (PROXY + 'Link2Bean.cpp', 'GetQueryValue(query, "fragment_fallback_delay")', 'query.queryItemValue("fragment_fallback_delay")'),
    'trojan-delay-pretty-getter': (PROXY + 'TrojanVLESSBean.cpp', 'GetQueryValue(query, "fragment_fallback_delay")', 'query.queryItemValue("fragment_fallback_delay")'),
    'vmess-delay-pretty-getter': (PROXY + 'VMessBean.cpp', 'GetQueryValue(query, "fragment_fallback_delay")', 'query.queryItemValue("fragment_fallback_delay")'),
    'quic-pretty-getter': (PROXY + 'QUICBean.cpp', 'GetQueryValue(query, "sni")', 'query.queryItemValue("sni")'),
    'quic-unprotected-export': (PROXY + 'QUICBean.cpp', 'query.addQueryItem("auth", QUrl::toPercentEncoding(authPayload))', 'query.addQueryItem("auth", authPayload)'),
    'quic-omit-empty-auth': (PROXY + 'QUICBean.cpp', 'query.addQueryItem("auth", QUrl::toPercentEncoding(authPayload))', 'add_query_nonempty("auth", query, authPayload)'),
    'fp-unprotected-export': (PROXY + 'Bean2Link.cpp', 'add_query_nonempty("fp", query, stream->utlsFingerprint)', 'query.addQueryItem("fp", stream->utlsFingerprint)'),
    'default-fp-unprotected-export': (PROXY + 'Bean2Link.cpp', 'add_query_nonempty("fp", query, Configs::dataStore->utlsFingerprint)', 'query.addQueryItem("fp", Configs::dataStore->utlsFingerprint)'),
    'security-unprotected-export': (PROXY + 'Bean2Link.cpp', 'add_query_nonempty("security", query, security == "" ? "none" : security)', 'query.addQueryItem("security", security == "" ? "none" : security)'),
    'encryption-unprotected-export': (PROXY + 'TrojanVLESSBean.cpp', 'add_query_nonempty("encryption", query, (encryption == "" ) ? "none" : encryption)', 'query.addQueryItem("encryption", (encryption == "" ) ? "none" : encryption)'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    parser.add_argument('--mutation', choices=MUTATIONS)
    parser.add_argument('--keep-generated', type=Path)
    args = parser.parse_args()
    hashes = {}

    def read(path):
        source = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
        if args.mutation:
            target, before, after = MUTATIONS[args.mutation]
            if path == target:
                if before not in source:
                    raise ValueError('Mutation anchor changed: ' + args.mutation)
                source = source.replace(before, after)
        hashes[path] = hashlib.sha256(source.encode()).hexdigest()
        return source

    utils = read('src/gharqad/dataStore/Utils.cpp')
    utils_h = read('src/nekobox/dataStore/Utils.hpp')
    extra = read(HEADER + 'AbstractBeanExtra.hpp')
    parse = read(PROXY + 'Link2Bean.cpp')
    export = read(PROXY + 'Bean2Link.cpp')
    stream = read(HEADER + 'V2RayStreamSettings.hpp')
    fields = section(stream, 'class V2rayStreamSettings : public JsonStore {', 'V2rayStreamSettings() : JsonStore()')
    code = (HERE / 'support.hpp').read_text().replace('STREAM_FIELDS', fields)
    code += '\nnamespace QtBase64 {' + section(utils_h, 'namespace QtBase64 {', 'namespace Qt515Base64 = QtBase64;') + '\nnamespace Qt515Base64 = QtBase64;\n'
    code += read('src/gharqad/dataStore/base64.cpp').replace('#include <nekobox/dataStore/Utils.hpp>', '')
    code += function(read('src/3rdparty/qv2ray/wrapper.hpp'), 'inline QJsonObject QString2QJsonObject(') + '\n'
    start = utils_h.index('#define GetQuery(url)')
    code += utils_h[start:utils_h.index('\n\n', start)] + '\n'
    start = extra.index('#define DECODE_V2RAY_N_1')
    code += extra[start:extra.index('\n\n', start)] + '\n'
    for signature in ('QString GetQueryValue(', 'int GetQueryIntValue(', 'QString SubStrBefore(', 'QString SubStrAfter(', 'QByteArray DecodeB64IfValid(',
                      'QJsonArray QListStr2QJsonArray(', 'QList<QString> QJsonArray2QListStr(', 'QJsonArray QString2QJsonArray(',
                      'QVariantMap QString2QMap(', 'QStringList GetQueryListValue(', 'QVariantMap GetQueryMapValue(',
                      'void AddQueryString(', 'void AddQueryStringList(', 'void AddQueryMap(', 'void AddQueryInt(', 'void AddQueryNatural('):
        code += function(utils, signature) + '\n'
    code += '\n' + read('src/3rdparty/URLParser/url_parser_function.h').replace('#pragma once', '')
    code += '\n' + read('src/3rdparty/URLParser/url_parser.h').replace('#pragma once', '').replace('#include \"url_parser_function.h\"', '')
    code += '\n#define FIRST_OR_SECOND(a, b) a.isEmpty() ? b : a\nnamespace Configs {\n'
    qheader = read(HEADER + 'QUICBean.hpp')
    code += qheader[qheader.index('class QUICBean : public AbstractBean {'):qheader.index('#undef _add')]
    code += 'QUICBean(ProxyEntity *, int); bool TryParseLink(const QString &); QString ToShareLink() const; };\n'
    for bean in BEANS:
        name = bean + 'Bean'
        filename = {'SSH': 'SSHBean.h', 'Wireguard': 'WireguardBean.h', 'Tailscale': 'Tailscale.hpp'}.get(bean, name + '.hpp')
        header = read(HEADER + filename)
        start = header.index('class ' + name + ' : public AbstractBean {')
        ctor_sig = name + '(Configs::ProxyEntity * entity'
        ctor = function(header, ctor_sig)
        prefix = header[start:header.index(ctor_sig, start)]
        prefix = re.sub(r'\bexplicit\s*$', '', prefix)
        code += prefix + ctor + '\npublic:\nbool TryParseLink(const QString &);\nQString ToShareLink() const;\n'
        if bean == 'Wireguard':
            # Explicitly bounded alternate-format boundary: fixture suite exercises
            # complete URI parser; native INI and Amnezia VPN imports are not tested.
            code += '''QString FormatReserved() const;
            bool parseAmneziaVpnLink(const QString &) { throw std::runtime_error("unexpected VPN import"); }
            bool parseWgConfig(QString value) { if (!value.startsWith("wg://") && !value.startsWith("awg://")) throw std::runtime_error("unexpected INI import"); return false; }
            '''
        code += '};\n'
    code += 'namespace From_Link {\n'
    for signature in ('void add_username_password(T * t, QUrl & url)', 'void add_udp_over_tcp(T *t, QUrlQuery & query)', 'void add_quic(T * bean, const QUrlQuery &query)', 'void add_network(T * t, const QUrlQuery &obj)'):
        code += 'template<typename T> ' + function(extra, signature) + '\n'
    code += '}\n'
    for signature in ('void From_Link::set_boolean(', 'void From_Link::add_tls(', 'void From_Link::add_default_fields(', 'void From_Link::add_mux_state('):
        code += function(parse, signature) + '\n'
    code += 'namespace To_Link {\n'
    for signature in ('void add_query_nonempty(', 'void add_query_int_natural(', 'void add_query_int(', 'void add_query_args_nonempty(', 'void add_query_map_nonempty(', 'void add_default_fields(', 'void add_query_boolean(', 'void add_mux_state(', 'void add_query_int_range(', 'void add_tls('):
        code += function(export, signature) + '\n'
    for signature, tp in (('void add_network(QUrlQuery & query, T * obj)', 'T'), ('inline void add_quic(QUrlQuery & q, B * bean)', 'B'), ('inline void add_udp_over_tcp(QUrlQuery & query, T * bean)', 'T'), ('inline void add_username_password(QUrl & url, B * bean)', 'B')):
        code += 'template<typename ' + tp + '> ' + function(extra, signature) + '\n'
    code += 'const inline char* fixShadowsocksUserNameEncodeMagic = "fixShadowsocksUserNameEncodeMagic-holder-for-QUrl";\n}\n'
    for bean in BEANS:
        source = read(PROXY + bean + 'Bean.cpp')
        code += function(source, 'bool ' + bean + 'Bean::TryParseLink(') + '\n'
        code += function(source, 'QString ' + bean + 'Bean::ToShareLink() const') + '\n'
        if bean == 'Wireguard':
            code += function(source, 'QString WireguardBean::FormatReserved() const') + '\n'
            code += 'void WireguardBean::enableAmnezia(bool value) { is_amnezia = value; }\n'
    quic = read(PROXY + 'QUICBean.cpp')
    for signature in ('QUICBean::QUICBean(', 'bool QUICBean::TryParseLink(', 'QString QUICBean::ToShareLink() const'):
        code += function(quic, signature) + '\n'
    expected = set(BEANS) | {'QUIC'}
    paths = subprocess.check_output(['git', '-C', str(args.repo), 'ls-tree', '-r', '--name-only', args.ref, '--', PROXY], text=True).splitlines() if args.ref else [str(p.relative_to(args.repo)) for p in (args.repo / PROXY).glob('*.cpp')]
    actual = set()
    for path in paths:
        source = read(path)
        if '::TryParseLink(' in source and ('GetQuery(url)' in source or 'QUrlQuery(url.query())' in source):
            actual.add(Path(path).stem.removesuffix('Bean'))
    if actual != expected:
        raise ValueError(f'Parser inventory changed: {actual ^ expected}')
    code += '}\n' + (HERE / 'checks.cpp').read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True)
    print('SOURCE:', args.ref or 'working tree', '| Mutation:', args.mutation or 'none', '| Qt:', subprocess.check_output(['pkg-config', '--modversion', 'Qt6Core'], text=True).strip(), flush=True)
    for path, digest in hashes.items():
        print(digest, path, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-query-decoding-') as temp:
        cpp = Path(temp) / 'test.cpp'
        exe = Path(temp) / 'test'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC']
        command += shlex.split(os.environ.get('EXTRA_CXXFLAGS', '')) + [str(cpp), '-o', str(exe)] + shlex.split(flags)
        subprocess.run(command, check=True, timeout=90)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
