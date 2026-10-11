#!/usr/bin/env python3
"""Compile actual refresh RPC helper/menu connection with synthetic RPC/UI."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=ROOT)
parser.add_argument('--ref')
parser.add_argument('--qt-root', type=Path)
args = parser.parse_args()

def read(path):
    if args.ref:
        return subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True)
    return (args.repo / path).read_text()

def segment(source, start, end):
    if source.count(start) != 1:
        raise SystemExit('Unexpected production anchor: ' + start)
    begin = source.index(start)
    return source[begin:source.index(end, begin)]

rpc = read('src/gharqad/ui/mainwindow_rpc.cpp')
window = read('src/gharqad/ui/mainwindow.cpp')
header = read('src/nekobox/ui/mainwindow.h')
method = segment(rpc, 'bool MainWindow::fetch_ruleset_cache(', '\nvoid MainWindow::runURLTest')
declarations = re.findall(r'bool fetch_ruleset_cache\(const QString &url\);', header)
if len(declarations) != 1:
    raise SystemExit('Expected one matching production fetch declaration')
declaration = declarations[0]
action = segment(window, '    auto *actionUpdateRuleSetCache =', '\n    auto *actionClearRuleSetCache =')
print('SOURCE', args.ref or 'working tree', hashlib.sha256((method + action).encode()).hexdigest(), flush=True)
fixture = (HERE / 'fixture.cpp.in').read_text().replace('@DECLARATION@', declaration).replace('@METHOD@', method).replace('@ACTION@', action)
with tempfile.TemporaryDirectory(prefix='nekobox-refresh-qt-') as directory:
    root = Path(directory)
    (root / 'fixture.cpp').write_text(fixture)
    compiler = shlex.split(os.environ.get('CXX', 'g++'))
    if args.qt_root:
        include = args.qt_root / 'usr/include/x86_64-linux-gnu/qt6'
        lib = args.qt_root / 'usr/lib/x86_64-linux-gnu'
        flags = [f'-I{include}', *[f'-I{include / item}' for item in ['QtCore', 'QtGui', 'QtWidgets']], f'-L{lib}', f'-Wl,-rpath,{lib}', '-l:libQt6Widgets.so.6', '-l:libQt6Gui.so.6', '-l:libQt6Core.so.6']
    else:
        flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Widgets'], text=True))
    command = compiler + ['-std=c++20', '-fPIC', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all', str(root / 'fixture.cpp'), '-o', str(root / 'test')] + flags
    subprocess.run(command, check=True, timeout=90)
    (root / 'runtime').mkdir(mode=0o700)
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen', XDG_RUNTIME_DIR=str(root / 'runtime'), XDG_CACHE_HOME=str(root / 'cache'), XDG_CONFIG_HOME=str(root / 'config'))
    if args.qt_root:
        env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(args.qt_root / 'usr/lib/x86_64-linux-gnu/qt6/plugins/platforms')
    raise SystemExit(subprocess.run([str(root / 'test')], env=env, timeout=10).returncode)
