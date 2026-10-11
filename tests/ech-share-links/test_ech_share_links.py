#!/usr/bin/env python3
"""Compile actual Trojan/VLESS share-link encoder and parser with real Qt6.

Requires Qt6Core development files, pkg-config and a C++20 compiler. Runs only
synthetic in-memory links. Enum and bean-storage boundaries are stand-ins;
actual function bodies, stream defaults, Qt URLs and queries are used.
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
sys.path.insert(0, str(HERE.parent))
from test_link_tls import function, section


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    parser.add_argument('--keep-generated', type=Path)
    args = parser.parse_args()
    hashes = {}
    def read(path):
        source = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
        hashes[path] = hashlib.sha256(source.encode()).hexdigest()
        return source
    utils = read('src/gharqad/dataStore/Utils.cpp')
    utils_h = read('src/nekobox/dataStore/Utils.hpp')
    parse = read('src/gharqad/configs/proxy/Link2Bean.cpp')
    export = read('src/gharqad/configs/proxy/Bean2Link.cpp')
    bean = read('src/gharqad/configs/proxy/TrojanVLESSBean.cpp')
    stream = read('src/nekobox/configs/proxy/V2RayStreamSettings.hpp')
    extra = read('src/nekobox/configs/proxy/AbstractBeanExtra.hpp')
    fields = section(stream, 'class V2rayStreamSettings : public JsonStore {', 'V2rayStreamSettings() : JsonStore()')
    code = (HERE / 'support.hpp').read_text().replace('STREAM_FIELDS', fields)
    start = utils_h.index('#define GetQuery(url)')
    code += '\n' + utils_h[start:utils_h.index('\n\n', start)] + '\n'
    code += function(utils, 'QString GetQueryValue(') + '\n' + function(utils, 'void AddQueryString(')
    code += '\nnamespace Configs {\nnamespace From_Link {\n'
    code += function(extra, 'void add_network(T * t, const QUrlQuery &obj)').replace('void add_network(', 'template<typename T> void add_network(', 1)
    code += '\n}\n'
    for signature in ('void From_Link::add_tls(', 'void From_Link::add_default_fields(', 'void From_Link::add_mux_state('):
        code += function(parse, signature) + '\n'
    code += 'namespace To_Link {\n'
    code += function(extra, 'void add_network(QUrlQuery & query, T * obj)').replace('void add_network(', 'template<typename T> void add_network(', 1)
    for signature in ('void add_query_nonempty(', 'void add_default_fields(', 'void add_query_boolean(', 'void add_mux_state(', 'void add_tls('):
        code += '\n' + function(export, signature)
    code += '\n}\n' + function(bean, 'bool TrojanVLESSBean::TryParseLink(')
    code += '\n' + function(bean, 'QString TrojanVLESSBean::ToShareLink() const') + '\n}\n'
    code += (HERE / 'checks.cpp').read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True)
    print('Source:', args.ref or 'working tree', '| Qt:', subprocess.check_output(['pkg-config', '--modversion', 'Qt6Core'], text=True).strip(), flush=True)
    for path, digest in hashes.items():
        print(digest, path, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-ech-share-') as temp:
        cpp = Path(temp) / 'test.cpp'
        exe = Path(temp) / 'test'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC']
        command += shlex.split(os.environ.get('EXTRA_CXXFLAGS', '')) + [str(cpp), '-o', str(exe)] + shlex.split(flags)
        subprocess.run(command, check=True, timeout=90)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
