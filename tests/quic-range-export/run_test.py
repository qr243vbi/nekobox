#!/usr/bin/env python3
"""Offline exporter-only checks of exact QUIC source using native Qt6."""
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

MUTATIONS = {
    'query-required': ('if (!portRange.isEmpty()) {', 'if (!portRange.isEmpty() && !query.isEmpty()) {'),
    'global-placeholder': ('link.replace(authorityEnd - 2, 2, ":" + portRange);', '(void)authorityEnd; link.replace(":0", ":" + portRange);'),
    'first-placeholder': ('link.replace(authorityEnd - 2, 2, ":" + portRange);', '(void)authorityEnd; link.replace(link.indexOf(":0"), 2, ":" + portRange);'),
    'wrong-length': ('link.replace(authorityEnd - 2, 2, ":" + portRange);', 'link.replace(authorityEnd - 2, 1, ":" + portRange);'),
    'discard-suffix': ('link.replace(authorityEnd - 2, 2, ":" + portRange);', 'link = link.left(authorityEnd - 2) + ":" + portRange;'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    parser.add_argument('--keep-generated', type=Path)
    parser.add_argument('--mutation', choices=MUTATIONS)
    args = parser.parse_args()
    hashes = {}

    def read(path):
        source = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
        if args.mutation and path == 'src/gharqad/configs/proxy/QUICBean.cpp':
            before, after = MUTATIONS[args.mutation]
            if source.count(before) != 1:
                raise ValueError('Mutation anchor changed: ' + args.mutation)
            source = source.replace(before, after)
        hashes[path] = hashlib.sha256(source.encode()).hexdigest()
        return source

    utils = read('src/gharqad/dataStore/Utils.cpp')
    extra = read('src/nekobox/configs/proxy/AbstractBeanExtra.hpp')
    export = read('src/gharqad/configs/proxy/Bean2Link.cpp')
    header = read('src/nekobox/configs/proxy/QUICBean.hpp')
    entity = read('src/nekobox/dataStore/ProxyEntity.hpp')
    bean = read('src/gharqad/configs/proxy/QUICBean.cpp')
    code = (HERE / 'support.hpp').read_text().replace('ENTITY_FIELDS', section(entity, 'QString display_type;', 'int id = -1;'))
    for sig in ('QString SubStrBefore(', 'QString SubStrAfter(', 'void AddQueryString(', 'void AddQueryInt(', 'void AddQueryNatural('):
        code += function(utils, sig) + '\n'
    code += '\nnamespace Configs {\n'
    code += header[header.index('class QUICBean : public AbstractBean {'):header.index('#undef _add')]
    code += 'QUICBean(ProxyEntity *, int); QString ToShareLink() const; };\nnamespace To_Link {\n'
    for sig in ('void add_query_int_natural(', 'void add_query_int(', 'void add_query_nonempty(', 'void add_default_fields(QUrl & url, const AbstractBean * bean)'):
        code += function(export, sig) + '\n'
    code += 'template<typename T> ' + function(extra, 'void add_network(QUrlQuery & query, T * obj)') + '\n}\n'
    code += function(bean, 'QUICBean::QUICBean(') + '\n'
    code += function(bean, 'QString QUICBean::ToShareLink() const') + '\n}\n'
    code += (HERE / 'checks.cpp').read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    print('SOURCE', args.ref or str(args.repo), 'Qt', subprocess.check_output(['pkg-config', '--modversion', 'Qt6Core'], text=True).strip(), flush=True)
    for path, digest in hashes.items():
        print(digest, path, flush=True)
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    with tempfile.TemporaryDirectory(prefix='nekobox-quic-export-') as temp:
        cpp, exe = Path(temp) / 'test.cpp', Path(temp) / 'test'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++'))
        command += ['-std=c++20', '-fPIC', '-Wall', '-Wextra', '-Werror', '-O0']
        command += shlex.split(os.environ.get('EXTRA_CXXFLAGS', '')) + [str(cpp), '-o', str(exe)] + flags
        subprocess.run(command, check=True, timeout=90)
        return subprocess.run([str(exe)], timeout=15).returncode


if __name__ == '__main__':
    raise SystemExit(main())
