#!/usr/bin/env python3
"""Reject deliberately broken production-source variants using runtime assertions."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SOURCE = ROOT / 'src/gharqad/configs/sub/GroupUpdater.cpp'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qt-root', type=Path)
    args = parser.parse_args()
    source = SOURCE.read_text()
    guard = 'if (!defaultOutboundTag.isEmpty() && routing["final"].toString().isEmpty())'
    mutations = {
        'overwrite-explicit-final': (guard, 'if (!defaultOutboundTag.isEmpty())'),
        'drop-implicit-fallback': (guard, 'if (false)'),
        'only-default-absent-not-empty': (guard, 'if (!defaultOutboundTag.isEmpty() && !routing.contains("final"))'),
        'choose-last-eligible': ('if (defaultOutboundTag.isEmpty() && type != "direct"', 'if (type != "direct"'),
        'choose-direct-as-fallback': ('type != "direct" && type != "block" &&', 'type != "block" &&'),
        'overwrite-after-sanitizer': ('auto cleanJson = sanitizeSingBoxConfig(json);', 'auto cleanJson = sanitizeSingBoxConfig(json); auto brokenRoute = cleanJson["route"].toObject(); brokenRoute["final"] = "proxy"; cleanJson["route"] = brokenRoute;'),
        'bypass-sanitizer-in-import': ('auto cleanJson = sanitizeSingBoxConfig(json);', 'auto cleanJson = json;'),
    }
    with tempfile.TemporaryDirectory(prefix='nekobox-route-final-mutations-') as td:
        for name, (before, after) in mutations.items():
            if source.count(before) != 1:
                raise SystemExit(f'Expected exactly one mutation anchor for {name}')
            path = Path(td) / f'{name}.cpp'
            path.write_text(source.replace(before, after))
            cmd = [sys.executable, str(HERE / 'test_route_final.py'), '--source', str(path)]
            if args.qt_root:
                cmd += ['--qt-root', str(args.qt_root)]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            if result.returncode != 1 or 'FAIL:' not in result.stderr or 'assertions passed;' not in result.stdout:
                print(result.stdout + result.stderr)
                raise SystemExit(f'Mutation did not fail through runtime assertions: {name}')
            print(f'CAUGHT {name}: ' + result.stdout.splitlines()[-1], flush=True)
    print(f'{len(mutations)}/{len(mutations)} production mutations rejected')


if __name__ == '__main__':
    main()
