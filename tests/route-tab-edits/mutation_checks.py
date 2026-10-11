#!/usr/bin/env python3
"""Mutation checks change temporary production sources, never the candidate."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
CPP = Path('src/gharqad/ui/setting/RouteItem.cpp')
HEADER = Path('src/nekobox/ui/setting/RouteItem.h')
MODEL = Path('src/gharqad/dataStore/RouteEntity.cpp')
FILES = [CPP, HEADER, MODEL, Path('src/nekobox/dataStore/RouteEntity.h'),
         Path('src/nekobox/ui/setting/RouteItem.ui'),
         Path('src/3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.cpp'),
         Path('src/3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.hpp')]


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f'Expected exactly one mutation target: {old}')
    return source.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qt-prefix', type=Path)
    parser.add_argument('--logs', required=True, type=Path)
    args = parser.parse_args()
    args.logs.mkdir(parents=True, exist_ok=True)
    mutations = [
        ('reload-dirty-draft', CPP, 'if (!simpleBoxesDirty) loadSimpleBoxes();', 'loadSimpleBoxes();'),
        ('rebuild-untouched-basic', CPP, '    if (!simpleBoxesDirty) return {};\n', ''),
        ('forget-success-reset', CPP, '    if (res.isEmpty()) simpleBoxesDirty = false;\n', ''),
        ('accept-invalid-draft', CPP, '    if (res.isEmpty()) simpleBoxesDirty = false;', '    simpleBoxesDirty = false;'),
        ('forget-load-reset', CPP, '    simpleBoxesDirty = false;\n}', '}'),
        ('cancel-shallow-copy', MODEL, 'Rules.push_back(std::make_shared<RouteRule>(*item));', 'Rules.push_back(item);'),
    ]
    # Each editor is also tested in isolation; editing the other eight must not
    # hide a missing textChanged connection for this one.
    loop = re.search(r'    for \(auto\* editor : \{[^}]+\}\) \{', (ROOT / CPP).read_text())
    if not loop:
        raise ValueError('Cannot locate editor connection loop')
    for editor in ['directIp', 'directDomain', 'directProcess', 'proxyIp', 'proxyDomain',
                   'proxyProcess', 'blockIp', 'blockDomain', 'blockProcess']:
        changed = re.sub(r'\b' + editor + r',\s*', '', loop[0], count=1)
        if changed == loop[0]:
            changed = re.sub(r',\s*' + editor + r'\b', '', loop[0], count=1)
        if changed == loop[0]:
            raise ValueError(f'Missing editor: {editor}')
        mutations.append((f'omit-{editor}-signal', CPP, loop[0], changed))

    def check(mutation):
        name, file, old, new = mutation
        with tempfile.TemporaryDirectory(prefix=f'route-tab-{name}-') as temporary:
            source = Path(temporary) / 'source'
            for path in FILES:
                (source / path).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / path, source / path)
            target = source / file
            target.write_text(replace_once(target.read_text(), old, new))
            command = [sys.executable, str(HERE / 'run_native_test.py'), '--source-root', str(source),
                       '--build-dir', str(Path(temporary) / 'build')]
            if args.qt_prefix:
                command += ['--qt-prefix', str(args.qt_prefix.resolve())]
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (args.logs / f'{name}.log').write_text(result.stdout)
            summaries = re.findall(r'SUMMARY cases=(\d+) failures=(\d+) Qt=([^\s]+)', result.stdout)
            caught = result.returncode == 1 and len(summaries) == 1 and int(summaries[0][1]) > 0
            print(('CAUGHT ' if caught else 'ERROR ') + name + (' ' + str(summaries[0]) if summaries else ''), flush=True)
            return caught

    with ThreadPoolExecutor(max_workers=3) as pool:
        outcomes = list(pool.map(check, mutations))
    print(f'SUMMARY mutations={len(outcomes)} caught={sum(outcomes)}')
    return 0 if all(outcomes) else 1


if __name__ == '__main__':
    raise SystemExit(main())
