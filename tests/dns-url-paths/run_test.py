#!/usr/bin/env python3
"""Real-Qt BuildDnsObject and optional pinned-core options/URL helper checks.

No transport, core, DNS lookup, HTTP request, profile or settings are created.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SOURCE = Path('src/gharqad/configs/ConfigBuilder.cpp')


def fixtures():
    rows = []
    # Escaped paths must remain encoded exactly once. Empty path uses the
    # pinned core's default; an explicit slash is the root endpoint.
    paths = [('', None, '/dns-query'), ('/dns-query', '/dns-query', '/dns-query'),
             ('/a/b', '/a/b', '/a/b'), ('/tenant/device/dns-query', '/tenant/device/dns-query', '/tenant/device/dns-query'),
             ('/', '/', '/'), ('/a/', '/a/', '/a/'), ('/a//b/', '/a//b/', '/a//b/'),
             ('//dns-query', '//dns-query', '//dns-query'),
             ('/a%2Fb/c%2fd', '/a%2Fb/c%2fd', '/a%2Fb/c%2fd'),
             ('/a%3Fb/c%23d', '/a%3Fb/c%23d', '/a%3Fb/c%23d'),
             ('/a%252Fb/c%2523d', '/a%252Fb/c%2523d', '/a%252Fb/c%2523d'),
             ('/a%20b/c+d', '/a%20b/c+d', '/a%20b/c+d'),
             ('/%E2%98%83/dns-query', '/%E2%98%83/dns-query', '/%E2%98%83/dns-query'),
             ('/a;v=1/b:2', '/a;v=1/b:2', '/a;v=1/b:2'),
             ('/https://example.invalid/h3://nested', '/https://example.invalid/h3://nested', '/https://example.invalid/h3://nested')]
    for scheme in ('https', 'h3'):
        for authority, server, port in [('dns.example', 'dns.example', None),
                                        ('dns.example:8443', 'dns.example', 8443),
                                        ('dns.example;8443', 'dns.example', 8443),
                                        ('[2001:db8::1]:8443', '2001:db8::1', 8443),
                                        ('[2001:db8::1]', '2001:db8::1', None)]:
            for suffix, path, uri in paths:
                expected = {'type': scheme, 'server': server, 'tls': {}}
                if port is not None:
                    expected['server_port'] = port
                if path is not None:
                    expected['path'] = path
                rows.append({'address': scheme + '://' + authority + suffix,
                             'expected': expected, 'uri': uri, 'category': 'path', 'standard_url': ';' not in authority})
        # Characterization only: literal query/fragment delimiters are NOT
        # supported URL components by core's path-only option. Do not claim
        # these outputs preserve complete URL semantics.
        for suffix, uri in [('/a/b?x=1', '/a/b%3Fx=1'),
                            ('/a/b#note', '/a/b%23note'),
                            ('/a/b?x=/c#note', '/a/b%3Fx=/c%23note')]:
            rows.append({'address': scheme + '://dns.example' + suffix,
                         'expected': {'type': scheme, 'server': 'dns.example', 'path': suffix, 'tls': {}},
                         'uri': uri, 'category': 'unsupported literal query/fragment'})
        for suffix in ('/a/%zz', '/a/%', '/a/%2'):
            rows.append({'address': scheme + '://dns.example' + suffix,
                         'expected': {'type': scheme, 'server': 'dns.example', 'path': suffix, 'tls': {}},
                         'path_error': True, 'category': 'invalid escape'})
    return [dict(row, tun=tun) for row in rows for tun in (False, True)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=ROOT)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--core', action='store_true', help='Also decode with pinned Go options and call the pure URLSetPath helper')
    parser.add_argument('--go', default=os.environ.get('GO', 'go'))
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    source = (args.source or args.repo / SOURCE).read_text()
    begin = 'QJsonObject BuildDnsObject(QString address, bool tunEnabled)'
    end = '\nQJsonObject BuildTunInbound'
    if source.count(begin) != 1 or source.count(end) != 1:
        raise SystemExit('Extraction anchors changed; review BuildDnsObject')
    function = source[source.index(begin):source.index(end)]
    qt = subprocess.run(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True, capture_output=True, check=True)
    fixture = '''#include <iostream>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
''' + function + '''
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    const auto obj = BuildDnsObject(QString::fromUtf8(line.c_str() + 1), line.at(0) == '1');
    std::cout << QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString() << '\\n';
  }
}
'''
    rows = fixtures()
    failed = 0
    with tempfile.TemporaryDirectory(prefix='nekobox-dns-paths-') as tmp:
        tmp = Path(tmp)
        unit = tmp / 'dns.cpp'
        unit.write_text(fixture)
        binary = tmp / 'dns-test'
        cmd = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++20', '-fPIC', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', str(unit), '-o', str(binary)]
        if args.sanitize:
            cmd += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
        subprocess.run(cmd + shlex.split(qt.stdout), check=True, timeout=120)
        result = subprocess.run([str(binary)], input=''.join(str(int(r['tun'])) + r['address'] + '\n' for r in rows), text=True, capture_output=True, check=True, timeout=30)
        actual = [json.loads(line) for line in result.stdout.splitlines()]
        if len(actual) != len(rows):
            raise SystemExit('Response count mismatch')
        for row, obj in zip(rows, actual):
            row['actual'] = obj
            if obj != row['expected']:
                failed += 1
                if failed <= 12:
                    print('FAIL', row['address'], 'tun=' + str(row['tun']), 'expected=', row['expected'], 'actual=', obj)
        print('FUNCTION_SHA256', hashlib.sha256(function.encode()).hexdigest(), flush=True)
        print(f'SUMMARY Qt checks={len(rows)} failures={failed}', flush=True)
        if args.core:
            # Reuse the repository's exact pinned module/replace graph without
            # touching its core directory or downloading modules/toolchains.
            for name in ('go.mod', 'go.sum'):
                (tmp / name).write_bytes((args.repo / 'core/server' / name).read_bytes())
            (tmp / 'main.go').write_bytes((HERE / 'core_options.go.in').read_bytes())
            env = dict(os.environ, GOPROXY='off', GOSUMDB='off', GOTOOLCHAIN='local', GOWORK='off', CGO_ENABLED='0')
            result = subprocess.run([args.go, 'run', '-mod=readonly', '.'], cwd=tmp, input=json.dumps(rows), text=True, env=env, timeout=180)
            failed += result.returncode != 0
    return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
