#!/usr/bin/env python3
"""Require runtime failures for temporary production-source mutations, offline."""
import argparse
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
NODE = 'src/gharqad/dataStore/ConfigData.cpp'
FILES = (
    NODE,
    'src/gharqad/dataStore/ConfigItem.cpp',
    'src/nekobox/dataStore/Configs.hpp',
    'src/gharqad/configs/proxy/Json2Bean.cpp',
    'src/gharqad/configs/proxy/Yaml2Bean.cpp',
)
MUTATIONS = (
    ('restore type predicate', 'return this->toNumber() > 0;', 'return this->isNumber() > 0;'),
    ('make zero true', 'return this->toNumber() > 0;', 'return this->toNumber() >= 0;'),
    ('treat negatives as true', 'return this->toNumber() > 0;', 'return this->toNumber() != 0;'),
    ('drop string conversion', 'if (isNumber() || isString() || isBoolean())', 'if (isNumber() || isBoolean())'),
    ('drop boolean conversion', 'if (isNumber() || isString() || isBoolean())', 'if (isNumber() || isString())'),
    ('sum collection instead of count', 'return count() > 0;', 'return toNumber() > 0;'),
    ('serialize false as true', 'return this->toBool();', 'return this->isBoolean();'),
)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qt-root', type=Path, default=os.environ.get('QT6_ROOT'))
    args = parser.parse_args()
    command = ['python3', str(HERE / 'test_node_boolean.py')]
    if args.qt_root:
        command += ['--qt-root', str(args.qt_root)]
    control = subprocess.run(command, capture_output=True, text=True, timeout=90)
    if control.returncode or ' checks; 0 failed' not in control.stdout:
        raise SystemExit('Unmutated control did not pass:\n' + control.stdout + control.stderr)
    print('control: ' + control.stdout.splitlines()[-1], flush=True)
    for name, old, new in MUTATIONS:
        with tempfile.TemporaryDirectory(prefix='nekobox-node-mutation-') as td:
            root = Path(td)
            for path in FILES:
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / path, target)
            target = root / NODE
            source = target.read_text()
            if source.count(old) != 1:
                raise AssertionError(f'Expected one mutation anchor: {name}')
            target.write_text(source.replace(old, new, 1))
            result = subprocess.run(command + ['--repo', str(root)], capture_output=True, text=True, timeout=90)
            if result.returncode != 1 or ' checks; ' not in result.stdout or 'FAIL:' not in result.stdout:
                raise SystemExit(f'Mutation did not produce a runtime assertion failure: {name}\n' + result.stdout + result.stderr)
            print(name + ': ' + result.stdout.splitlines()[-1], flush=True)
    print(f'PASS: all {len(MUTATIONS)} production-source mutations detected', flush=True)

if __name__ == '__main__':
    main()
