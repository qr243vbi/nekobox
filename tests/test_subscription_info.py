#!/usr/bin/env python3
"""Exercise verbatim subscription display helpers using real Qt6 Core, offline.

Run: python3 tests/test_subscription_info.py [--ref COMMIT]
Requires Python 3, a C++20 compiler and Qt6Core pkg-config development files.
No application, network, subscription, profile, settings or core is opened.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent / 'subscription-info'


def function(source, signature):
    if source.count(signature) != 1:
        raise ValueError(f'Expected one function: {signature}')
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for pos in range(brace, len(source)):
        depth += (source[pos] == '{') - (source[pos] == '}')
        if depth == 0:
            return source[start:pos + 1]
    raise ValueError(f'Unterminated function: {signature}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=ROOT)
    parser.add_argument('--ref', help='Read immutable production sources from this Git ref')
    args = parser.parse_args()

    def read(path):
        if args.ref:
            return subprocess.check_output(['git', '-C', str(args.repo), 'show',
                                            f'{args.ref}:{path}'], text=True, timeout=10)
        return (args.repo / path).read_text(encoding='utf-8')

    source = read('src/gharqad/ui/group/GroupItem.cpp')
    utils = read('src/gharqad/dataStore/Utils.cpp')
    configs = read('src/nekobox/dataStore/Configs.hpp')
    enums = read('src/gharqad/dataStore/ConfigItem.cpp')
    request = read('src/gharqad/dataStore/HTTPRequestHelper.cpp')
    header_helpers = configs[configs.index('struct EnumFieldName {'):configs.index('// Custom hasher')]
    header_helpers += enums[enums.index('EnumFieldName::EnumFieldName() = default;'):enums.index('// --- Hasher and equality functors ---')]
    header_helpers += '\ntemplate<typename B, typename C>\n' + function(configs,
        'inline QMap<EnumFieldName, QString> QStdMapString2QMapEnumFieldName(')
    header_helpers += '\nstruct NetworkRequestHelper { static QString GetHeader(const QMap<EnumFieldName, QString> &, const QString &); };\n'
    header_helpers += function(request, 'QString NetworkRequestHelper::GetHeader(')
    extracted = '\n'.join((function(utils, 'QString ReadableSize('),
                           function(utils, 'QString DisplayTime('),
                           function(source, 'QString ParseSubInfo(')))
    code = '''#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QObject>
#include <QLocale>
#include <QDateTime>
#include <QMap>
#include <map>
#include <tuple>
#include <utility>
#include <iostream>
#include <limits>
#include <string>
''' + extracted + '\n' + header_helpers + '\n' + (HERE / 'checks.cpp').read_text(encoding='utf-8')
    flags = shlex.split(subprocess.check_output(
        ['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    print('SOURCE:', args.ref or 'working tree', '| SHA256:',
          hashlib.sha256(extracted.encode()).hexdigest(), flush=True)
    print('Qt6 Core:', subprocess.check_output(
        ['pkg-config', '--modversion', 'Qt6Core'], text=True).strip(), flush=True)
    count = len(re.findall(r'^    \{"', (HERE / 'checks.cpp').read_text(), re.MULTILINE))
    with tempfile.TemporaryDirectory(prefix='nekobox-subscription-info-') as directory:
        cpp = Path(directory) / 'checks.cpp'
        binary = Path(directory) / 'checks'
        cpp.write_text(code, encoding='utf-8')
        command = shlex.split(os.environ.get('CXX', 'c++')) + [
            '-std=c++20', '-fPIC', '-Wall', '-Wextra', '-Werror', '-O0',
            '-fsanitize=undefined', '-fno-sanitize-recover=all',
            str(cpp), '-o', str(binary), *flags]
        subprocess.run(command, check=True, timeout=60)
        env = dict(os.environ, TZ='UTC')
        failed = 0
        for case in [*range(count), 'headers']:
            result = subprocess.run([str(binary), str(case)], env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, timeout=10)
            print(result.stdout, end='', flush=True)
            failed += result.returncode != 0
        print(f'SUMMARY cases={count} + header checks=5 failures={failed}', flush=True)
        return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
