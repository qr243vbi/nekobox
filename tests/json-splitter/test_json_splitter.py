#!/usr/bin/env python3
"""Run the production JSON splitter and import dispatch with native Qt6 Core.

No application, profile storage, proxy core, or network is used. Explicit test boundaries
substitute unrelated RawUpdater dependencies only.
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
    if text.count(start) != 1 or text.count(end) != 1:
        raise ValueError(f'Source anchors changed: {start!r}, {end!r}')
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref', help='Read production source from an immutable Git revision')
    parser.add_argument('--keep-generated', type=Path)
    parser.add_argument('--splitter-only', action='store_true', help='Skip isolated importer cases')
    args = parser.parse_args()
    source = (subprocess.check_output(['git', '-C', str(args.repo), 'show',
               f'{args.ref}:{SOURCE}'], text=True) if args.ref else (args.repo / SOURCE).read_text())
    bodies = section(source, 'int JsonEndIdx(', 'static QString jsonFirstString(')
    bodies += section(source, 'void RawUpdater::update(const QString &str3)',
                      'void RawUpdater::updateSIP008(')
    bodies += section(source, 'bool RawUpdater::AddProxy(', 'void GroupUpdater::AsyncUpdateGroup(')
    code = (HERE / 'support.hpp').read_text()
    code += '\nnamespace Subscription {\n' + bodies + '\n}\n'
    code += (HERE / 'checks.cpp').read_text()
    probe = subprocess.run(['pkg-config', '--cflags', '--libs', 'Qt6Core'],
                           text=True, capture_output=True)
    if probe.returncode:
        raise SystemExit('BLOCKED: Qt6 Core development headers/pkg-config metadata unavailable.')
    if args.keep_generated:
        args.keep_generated.write_text(code)
    print('SOURCE:', args.ref or 'working tree', 'SHA256:', hashlib.sha256(source.encode()).hexdigest(), flush=True)
    print('MODE: native Qt6 Core; extracted splitter/import control flow; substituted unrelated dependencies', flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-json-splitter-') as td:
        cpp, binary = Path(td) / 'regression.cpp', Path(td) / 'regression'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++'))
        command += ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-unused-variable',
                    '-O0', '-DUSE_REAL_QT', '-fPIC']
        command += shlex.split(os.environ.get('EXTRA_CXXFLAGS', ''))
        command += [str(cpp), '-o', str(binary)] + shlex.split(probe.stdout)
        subprocess.run(command, check=True, timeout=60)
        result = subprocess.run([str(binary)], timeout=30).returncode
        if args.splitter_only:
            return result
        import_count = int(subprocess.check_output([str(binary), '--count-imports'], text=True))
        import_failures = 0
        # Isolate each importer fixture: the old splitter can make the existing
        # multiline retry loop repeat a truncated object indefinitely.
        for index in range(import_count):
            try:
                run = subprocess.run([str(binary), str(index)], timeout=2,
                                     text=True, capture_output=True)
                if run.returncode:
                    import_failures += 1
                    print(f'FAIL import case {index}: {run.stdout.strip()}', flush=True)
            except subprocess.TimeoutExpired:
                import_failures += 1
                print(f'FAIL import case {index}: exceeded 2 seconds', flush=True)
        print(f'IMPORT CASES: {import_count - import_failures}/{import_count} passed', flush=True)
        return int(result != 0 or import_failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
