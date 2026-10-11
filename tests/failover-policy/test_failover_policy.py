#!/usr/bin/env python3
"""Compile actual failover policy with Qt6Core and inert application boundaries.

No NekoBox application, core, profile, RPC, health request, or settings store is
created or loaded. --ref checks a committed source; --mutation checks test power.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = 'src/gharqad/stats/autotester/ProxyAutoTester.cpp'
BASELINE = 'a536846c9c74dd8fb104dce3319cbd9672920bb3'
SIGNATURES = (
    'void ProxyAutoTester::HandleProxyFailure(',
    'int ProxyAutoTester::GetNextWorkingProxy(',
    'bool ProxyAutoTester::isTunFailoverEnabled(',
    'int ProxyAutoTester::getPoolSize(',
    'void ProxyAutoTester::pruneWorkingPool(',
)


def extract_function(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f'Expected exactly one function: {signature}')
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for end in range(brace, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if depth == 0:
            return source[start:end + 1]
    raise ValueError(f'Unterminated function: {signature}')


def mutate(source, name):
    gate = '(!Configs::dataStore->spmode_vpn || isTunFailoverEnabled())'
    if name == 'ignore-checkbox':
        old, new = gate, 'true'
    elif name == 'gate-proxy-too':
        old, new = gate, 'isTunFailoverEnabled()'
    elif name == 'invert-checkbox':
        old, new = gate, '(!Configs::dataStore->spmode_vpn || !isTunFailoverEnabled())'
    elif name == 'ignore-active-id':
        old, new = 'Configs::dataStore->started_id == proxyId', 'true'
    elif name == 'skip-failure-bookkeeping':
        old = 'void ProxyAutoTester::HandleProxyFailure(int proxyId, int attemptCount) {'
        new = old + '\n    if (Configs::dataStore->spmode_vpn && !isTunFailoverEnabled()) return;'
    elif name == 'ignore-setting-value':
        old, new = 'return Configs::dataStore->auto_test_tun_failover;', 'return true;'
    else:
        raise ValueError(name)
    if source.count(old) != 1:
        raise ValueError(f'Mutation anchor changed: {old}')
    return source.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    parser.add_argument('--qt-root', type=Path)
    parser.add_argument('--keep-generated', type=Path)
    parser.add_argument('--mutation', choices=(
        'ignore-checkbox', 'gate-proxy-too', 'invert-checkbox', 'ignore-active-id',
        'skip-failure-bookkeeping', 'ignore-setting-value'))
    args = parser.parse_args()
    source = subprocess.check_output(
        ['git', '-C', str(args.repo), 'show', f'{args.ref}:{SOURCE}'], text=True
    ) if args.ref else (args.repo / SOURCE).read_text()
    print('Source:', args.ref or 'working tree', flush=True)
    print('SHA256:', hashlib.sha256(source.encode()).hexdigest(), SOURCE, flush=True)
    bodies = '\n\n'.join(extract_function(source, signature) for signature in SIGNATURES)
    if args.mutation:
        bodies = mutate(bodies, args.mutation)
        print('MUTATION:', args.mutation, flush=True)
    code = (HERE / 'support.hpp').read_text() + '\nnamespace Stats {\n' + bodies + '\n}\n' + (HERE / 'checks.cpp').read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    environment = os.environ.copy()
    if args.qt_root:
        root = args.qt_root.resolve()
        lib = root / 'usr/lib/x86_64-linux-gnu'
        environment['PKG_CONFIG_SYSROOT_DIR'] = str(root)
        environment['PKG_CONFIG_LIBDIR'] = str(lib / 'pkgconfig')
        environment['LD_LIBRARY_PATH'] = str(lib) + ':' + environment.get('LD_LIBRARY_PATH', '')
    qt = shlex.split(subprocess.check_output(
        ['pkg-config', '--cflags', '--libs', 'Qt6Core'], env=environment, text=True))
    print('Qt6Core:', subprocess.check_output(
        ['pkg-config', '--modversion', 'Qt6Core'], env=environment, text=True).strip(), flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-failover-policy-') as temporary:
        cpp = Path(temporary) / 'policy.cpp'
        binary = Path(temporary) / 'policy-test'
        cpp.write_text(code)
        command = shlex.split(environment.get('CXX', 'c++')) + [
            '-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC',
            str(cpp), '-o', str(binary)] + qt
        subprocess.run(command, env=environment, check=True, timeout=90)
        return subprocess.run([str(binary)], env=environment, timeout=10).returncode


if __name__ == '__main__':
    raise SystemExit(main())
