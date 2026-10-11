#!/usr/bin/env python3
"""Check regression sensitivity in disposable source copies; never run the app."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
MAIN = Path('src/gharqad/ui/mainwindow_rpc.cpp')
RPC = Path('src/gharqad/api/RPC.cpp')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parent.parent)
    parser.add_argument('--qt-prefix', type=Path)
    args = parser.parse_args()
    original = (args.repo / MAIN).read_text()
    mutations = {
        'drop-enable-error': ('error = defaultClient->EnableSystemProxy(',
                              'defaultClient->EnableSystemProxy('),
        'drop-disable-error': ('error = defaultClient->DisableSystemProxy(',
                               'defaultClient->DisableSystemProxy('),
        'accept-service-error': ('        isok = false;\n    }\n    #ifdef DEBUG_MODE',
                                 '    }\n    #ifdef DEBUG_MODE'),
        'hide-service-error': ('        MW_show_log(tr("Failed to set system proxy: ") + error);', ''),
        'lose-previous-state': ('enable = Configs::dataStore->spmode_system_proxy;', 'enable = true;'),
    }
    for name, (before, after) in mutations.items():
        if original.count(before) != 1:
            raise ValueError(f'{name}: expected one mutation anchor')
        with tempfile.TemporaryDirectory(prefix='nekobox-proxy-mutation-') as tmp:
            root = Path(tmp)
            for path in (MAIN, RPC):
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                source = original.replace(before, after, 1) if path == MAIN else (args.repo / path).read_text()
                (root / path).write_text(source)
            command = [sys.executable, str(HERE.parent / 'test_system_proxy_result.py'), '--repo', str(root)]
            if args.qt_prefix:
                command += ['--qt-prefix', str(args.qt_prefix)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=180)
            if result.returncode != 1 or 'FAIL ' not in result.stdout or result.stdout.count('SUMMARY checks=') != 3:
                print(result.stdout + result.stderr)
                raise SystemExit(f'FAIL mutation was not rejected by assertions: {name}')
            print(f'PASS mutation rejected: {name}', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
