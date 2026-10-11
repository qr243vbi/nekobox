#!/usr/bin/env python3
"""Compile the production stdout/stderr callbacks with real Qt6 Core strings.

Only the callbacks execute: process I/O and GUI actions are synthetic. No real
process launch, restart, network, settings, or application lifecycle is tested.
Requires Python 3, pkg-config, Qt6 Core development files, and GCC/Clang C++20.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def read(repo, ref, path):
    if ref:
        return subprocess.check_output(['git', '-C', str(repo), 'show', f'{ref}:{path}'], text=True)
    return (repo / path).read_text(encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parent.parent)
    parser.add_argument('--ref', help='Read production code from an immutable revision')
    args = parser.parse_args()
    source = read(args.repo, args.ref, 'src/gharqad/sys/Process.cpp')
    header = read(args.repo, args.ref, 'src/nekobox/sys/Process.hpp')
    begin = '        auto maxLogLines = []() {'
    end = '        connect(&process, &QProcess::errorOccurred'
    if source.count(begin) != 1 or source.count(end) != 1:
        raise SystemExit('Production callback layout changed; review extraction.')
    connections = source[source.index(begin):source.index(end)]
    profile = re.findall(r'^        int start_profile_when_core_is_up = .*?;$', header, re.M)
    byte_fields = re.findall(r'^        QByteArray .*?;$', header, re.M)
    if len(profile) != 1:
        raise SystemExit('Production profile declaration changed; review extraction.')
    fields = profile + byte_fields
    # This is intentionally a static placement check, not a lifecycle test.
    # Never execute Start()/Restart() or reconstruct their side effects here.
    if 'core_readiness_tail' in header:
        start = source.split('    void CoreProcess::Start() {', 1)[1].split('    void CoreProcess::Restart()', 1)[0]
        guard = start.index('if (started) return;')
        reset = start.find('core_readiness_tail.clear();')
        launch = start.index('process.start();')
        if not guard < reset < launch or start.count('core_readiness_tail.clear();') != 1:
            raise SystemExit('Readiness carry must reset after the Start guard and before launch.')
        print('STATIC: readiness carry reset appears after Start guard and before launch; no lifecycle executed.', flush=True)
    bounded = ('        check(f.core.core_readiness_tail.size() < marker.size(), "readiness carry remains bounded");'
               if 'core_readiness_tail' in header else '')
    fixture = (HERE / 'fixture.cpp.in').read_text(encoding='utf-8')
    cpp = fixture.replace('@STATE@', '\n'.join(fields)).replace('@CONNECTIONS@', connections).replace('@BOUNDED_CHECK@', bounded)
    if re.search(r'@[A-Z_]+@', cpp):
        raise SystemExit('Unexpanded fixture token.')
    probe = subprocess.run(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True, capture_output=True)
    if probe.returncode:
        raise SystemExit('BLOCKED: Qt6 Core development files unavailable to pkg-config. ' + probe.stderr)
    print('SOURCE', args.ref or 'working tree', 'Qt', subprocess.check_output(['pkg-config', '--modversion', 'Qt6Core'], text=True).strip(), flush=True)
    print('CALLBACK_SHA256', hashlib.sha256(connections.encode()).hexdigest(), flush=True)
    print('HEADER_SHA256', hashlib.sha256(header.encode()).hexdigest(), flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-core-readiness-') as tmp:
        path = Path(tmp)
        (path / 'fixture.cpp').write_text(cpp, encoding='utf-8')
        binary = path / 'regression'
        command = shlex.split(os.environ.get('CXX', 'c++'))
        command += ['-std=c++20', '-fPIC', '-Wall', '-Wextra', '-Werror', '-O0', str(path / 'fixture.cpp'), '-o', str(binary)]
        command += shlex.split(probe.stdout)
        subprocess.run(command, check=True, timeout=60)
        return subprocess.run([str(binary)], timeout=20).returncode


if __name__ == '__main__':
    raise SystemExit(main())
