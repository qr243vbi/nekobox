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
    'void ProxyAutoTester::Stop()',
    'bool ProxyAutoTester::isEnabled() const',
    'bool ProxyAutoTester::canDeliverFailover(',
    'bool ProxyAutoTester::isBackupAvailable(',
    'QList<int> ProxyAutoTester::GetWorkingProxies(',
    'bool ProxyAutoTester::IsProxyWorking(',
    'void ProxyAutoTester::updateWorkingPool(',
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
    elif name == 'ignore-delivery-policy':
        old = extract_function(source, 'bool ProxyAutoTester::canDeliverFailover(')
        new = 'bool ProxyAutoTester::canDeliverFailover(int, quint64) const { return true; }'
    elif name == 'ignore-backup-validity':
        old = 'if (!guard->isBackupAvailable(nextProxyId)) return;'
        new = '/* backup checks removed */'
    elif name == 'stop-keeps-generation':
        old, new = '++runGeneration;', '/* no generation change */'
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
        'skip-failure-bookkeeping', 'ignore-setting-value', 'ignore-delivery-policy',
        'ignore-backup-validity', 'stop-keeps-generation'))
    args = parser.parse_args()
    source = subprocess.check_output(
        ['git', '-C', str(args.repo), 'show', f'{args.ref}:{SOURCE}'], text=True
    ) if args.ref else (args.repo / SOURCE).read_text()
    print('Source:', args.ref or 'working tree', flush=True)
    print('SHA256:', hashlib.sha256(source.encode()).hexdigest(), SOURCE, flush=True)
    bodies = '\n\n'.join(extract_function(source, signature) for signature in SIGNATURES
                         if signature in source)
    if args.mutation:
        bodies = mutate(bodies, args.mutation)
        print('MUTATION:', args.mutation, flush=True)
    gui_path = 'src/gharqad/global/GuiUtils.cpp'
    gui = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{gui_path}'], text=True) if args.ref else (args.repo / gui_path).read_text()
    post = extract_function(gui, 'void runOnUiThread(').replace('void runOnUiThread(', 'void postProductionUiThread(', 1)
    feature = '#define HAS_BACKUP_VALIDATION\n' if 'bool ProxyAutoTester::isBackupAvailable(' in source else ''
    code = feature + (HERE / 'support.hpp').read_text() + '\n' + post + '\nnamespace Stats {\n' + bodies + '\n}\n' + (HERE / 'checks.cpp').read_text()
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
