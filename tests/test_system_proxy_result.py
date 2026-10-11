#!/usr/bin/env python3
"""Compile the production system-proxy result handling against safe boundaries.

Run: python3 tests/test_system_proxy_result.py [--repo PATH] [--ref COMMIT]
Requires Python 3, a C++20 compiler, and Qt6Core development files. An optional
--qt-prefix accepts an already-extracted Debian-style Qt installation.

The production MainWindow method, RPC wrappers, and RPC status/check/log helpers
are extracted verbatim. Only CHANNEL is substituted with synthetic responses.
Settings persistence and UI refresh are in-memory observers; the legacy proxy
configurator is a call recorder. No socket, OS proxy API, app, or core is run.
This is focused result-contract coverage, not an OS/GUI integration test.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def function(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f'Expected one function anchor: {signature}')
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for end in range(brace, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if depth == 0:
            return source[start:end + 1]
    raise ValueError(f'Unterminated function: {signature}')


def macro(source, name):
    start = '#define ' + name
    lines = source.splitlines()
    matches = [i for i, line in enumerate(lines) if line.startswith(start)]
    if len(matches) != 1:
        raise ValueError(f'Expected one macro: {name}')
    end = begin = matches[0]
    while lines[end].rstrip().endswith('\\'):
        end += 1
    return '\n'.join(lines[begin:end + 1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parent)
    parser.add_argument('--ref', help='Read immutable Git revision instead of working files')
    parser.add_argument('--qt-prefix', type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()

    def read(path):
        if args.ref:
            return subprocess.check_output(['git', '-C', str(repo), 'show', f'{args.ref}:{path}'], text=True)
        return (repo / path).read_text(encoding='utf-8')

    rpc = read('src/gharqad/api/RPC.cpp')
    mainwindow = read('src/gharqad/ui/mainwindow_rpc.cpp')
    chunks = {
        'STATUS': function(rpc, 'struct Status') + ';',
        'CHECK': macro(rpc, 'CHECK('),
        'NOT_OK': macro(rpc, 'NOT_OK'),
        'RPC': '\n\n'.join(function(rpc, signature) for signature in (
            'void Client::onError(', 'QString Client::EnableSystemProxy(',
            'QString Client::DisableSystemProxy(')),
        'MAINWINDOW': function(mainwindow, 'bool MainWindow::set_spmode_system_proxy('),
    }
    fixture = (HERE / 'system-proxy-result/fixture.cpp.in').read_text(encoding='utf-8')
    for name, code in chunks.items():
        marker = '@' + name + '@'
        if fixture.count(marker) != 1:
            raise ValueError(f'Expected one fixture marker: {marker}')
        fixture = fixture.replace(marker, code)
    if args.qt_prefix:
        prefix = args.qt_prefix.resolve()
        include = prefix / 'usr/include/x86_64-linux-gnu/qt6'
        lib = prefix / 'usr/lib/x86_64-linux-gnu'
        flags = [f'-I{include}', f'-I{include / "QtCore"}', f'-L{lib}',
                 f'-Wl,-rpath,{lib}', '-l:libQt6Core.so.6']
    else:
        flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    print('SOURCE:', args.ref or 'working tree', flush=True)
    for name, code in chunks.items():
        print(name, 'SHA256', hashlib.sha256(code.encode()).hexdigest(), flush=True)
    result = 0
    with tempfile.TemporaryDirectory(prefix='nekobox-proxy-result-') as tmp:
        cpp = Path(tmp) / 'regression.cpp'
        cpp.write_text(fixture, encoding='utf-8')
        for mode in ('rpc', 'cpp-configurator', 'rpc-debug'):
            binary = Path(tmp) / mode
            defines = {'rpc': [], 'cpp-configurator': ['-DUSE_CPP_PROXY_CONFIGURATOR'],
                       'rpc-debug': ['-DDEBUG_MODE']}[mode]
            command = shlex.split(os.environ.get('CXX', 'g++'))
            command += ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC',
                        *defines, str(cpp), '-o', str(binary), *flags]
            print('MODE:', mode, flush=True)
            subprocess.run(command, check=True, timeout=60)
            result |= subprocess.run([str(binary)], timeout=10).returncode
    return result


if __name__ == '__main__':
    raise SystemExit(main())
