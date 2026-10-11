#!/usr/bin/env python3
"""Run real Qt6 JSON operations and verbatim importer sanitizer/lambda bodies.

No application, core, profile store, sockets or OS routing operations are used.
The only substitutes are the settings object and in-memory profile boundary.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = Path('src/gharqad/configs/sub/GroupUpdater.cpp')


def section(text, start, end):
    if text.count(start) != 1:
        raise ValueError(f'Expected one start anchor: {start}')
    at = text.index(start)
    return text[at:text.index(end, at)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref', help='Read immutable production source with git show')
    parser.add_argument('--source', type=Path, help='Temporary mutated production source')
    parser.add_argument('--qt-root', type=Path, default=os.environ.get('QT6_ROOT'))
    parser.add_argument('--keep-generated', type=Path)
    args = parser.parse_args()
    if args.source and args.ref:
        parser.error('--source and --ref are mutually exclusive')
    if args.source:
        source = args.source.read_text()
    elif args.ref:
        source = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{SOURCE}'], text=True)
    else:
        source = (args.repo / SOURCE).read_text()
    helpers = section(source, 'static QString jsonFirstString(', 'void RawUpdater::update(const QString &str3)')
    importer = section(source, '  auto addFullJsonProxy = ', 'ret_loop:')
    code = (HERE / 'support.hpp').read_text() + '\nnamespace Subscription {\n' + helpers
    code += '\nstruct Importer {\nstd::shared_ptr<Configs::ProxyEntity> ent;\n'
    code += 'std::vector<std::shared_ptr<Configs::ProxyEntity>> profiles;\n'
    code += 'void AddProxy(const std::shared_ptr<Configs::ProxyEntity> &value) { profiles.push_back(value); }\n'
    code += 'void add(const QJsonObject &json, const QString &remarks) {\n' + importer
    code += '\naddFullJsonProxy(json, remarks);\n}\n};\n}\n' + (HERE / 'checks.cpp').read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    if args.qt_root:
        include = args.qt_root / 'usr/include/x86_64-linux-gnu/qt6'
        lib = args.qt_root / 'usr/lib/x86_64-linux-gnu'
        flags = [f'-I{include}', f'-I{include}/QtCore', f'-L{lib}', f'-Wl,-rpath,{lib}', '-l:libQt6Core.so.6']
    else:
        qt = subprocess.run(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True, capture_output=True)
        if qt.returncode:
            raise SystemExit('BLOCKED: Qt6Core development files unavailable; supply --qt-root or install in your build environment.')
        flags = shlex.split(qt.stdout)
    with tempfile.TemporaryDirectory(prefix='nekobox-route-final-') as td:
        cpp, exe = Path(td) / 'test.cpp', Path(td) / 'test'
        cpp.write_text(code)
        cmd = shlex.split(os.environ.get('CXX', 'c++'))
        cmd += ['-std=c++20', '-fPIC', '-Wall', '-Wextra', '-Werror', '-O0']
        cmd += shlex.split(os.environ.get('EXTRA_CXXFLAGS', ''))
        cmd += [str(cpp), '-o', str(exe)] + flags
        print('SOURCE:', args.source or args.ref or 'working tree', 'SHA256:', hashlib.sha256(source.encode()).hexdigest(), flush=True)
        subprocess.run(cmd, check=True, timeout=90)
        return subprocess.run([str(exe)], timeout=20).returncode


if __name__ == '__main__':
    raise SystemExit(main())
