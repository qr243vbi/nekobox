#!/usr/bin/env python3
"""Offline real Qt production emission -> actual pinned Go option decoding."""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from test_link_tls import function, section

PRODUCTION = (
    'src/gharqad/dataStore/Utils.cpp',
    'src/nekobox/dataStore/Utils.hpp',
    'src/nekobox/configs/proxy/V2RayStreamSettings.hpp',
    'src/gharqad/configs/proxy/Bean2CoreObj_box.cpp',
    'src/nekobox/configs/proxy/AbstractBeanExtra.hpp',
    'core/server/go.mod',
    'core/server/go.sum',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref', help='Read production sources from an immutable revision')
    parser.add_argument('--artifacts', type=Path, help='Keep generated C++, JSON and source hashes')
    args = parser.parse_args()
    hashes = {}

    def read(path):
        source = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
        hashes[path] = hashlib.sha256(source.encode()).hexdigest()
        return source

    utils, utils_h, stream, core, extra, go_mod, go_sum = map(read, PRODUCTION)
    fields = section(stream, 'class V2rayStreamSettings : public JsonStore {', 'V2rayStreamSettings() : JsonStore()')
    kcp = section(stream, 'class KCPExtra: public JsonStore {', 'KCPExtra(){};')
    code = (HERE / 'support.hpp').read_text().replace('STREAM_FIELDS', fields).replace('KCP_FIELDS', kcp).replace('HEADER_PAIRS', function(stream, 'QMap<QString, QString> GetHeaderPairs('))
    code += '\n' + section(utils_h, '#include <utility>\n', '\n\nQString Quote(')
    for signature in ('QString SubStrBefore(', 'QString SubStrAfter(', 'QJsonArray QListStr2QJsonArray(', 'QJsonObject QMapString2QJsonObject('):
        code += '\n' + function(utils, signature)
    code += '\nnamespace Configs { namespace CoreObj_box {\n'
    code += 'QJsonObject getXbadoptionRange(const QJsonValue &);\nQJsonObject getXmux(const QJsonValue &);\nint domainStrategy(const QJsonValue &);\n'
    code += 'void parseExtraXhttp(QJsonObject &, QString);\nvoid parseExtraKCP(QJsonObject &, std::shared_ptr<KCPExtra>);\n'
    code += 'template<typename T>\n' + function(extra, 'inline void add_non_empty(T & obj, const QString & key, const QString & value)') + '\n}\n'
    for signature in ('QJsonObject CoreObj_box::getXmux(', 'int CoreObj_box::domainStrategy(', 'QJsonObject CoreObj_box::getXbadoptionRange(', 'void CoreObj_box::parseExtraKCP(', 'void CoreObj_box::parseExtraXhttp('):
        code += function(core, signature) + '\n'
    code += 'using namespace CoreObj_box;\n' + function(core, 'void V2rayStreamSettings::BuildStreamSettingsSingBox(') + '\n}\n'
    code += (HERE / 'checks.cpp').read_text()
    qt_flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    print('SOURCE:', args.ref or 'working tree', '| real Qt6 emitter and pinned Go option decoder', flush=True)
    hash_text = ''.join(f'{digest} {path}\n' for path, digest in hashes.items())
    print(hash_text, end='', flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-xhttp-padding-') as temp:
        root = Path(temp)
        cpp = root / 'emitter.cpp'
        binary = root / 'emitter'
        fixtures = root / 'fixtures.json'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC']
        command += shlex.split(os.environ.get('EXTRA_CXXFLAGS', '')) + [str(cpp), '-o', str(binary)] + qt_flags
        subprocess.run(command, check=True, timeout=90)
        qt_result = subprocess.run([str(binary), str(fixtures)], timeout=10)
        if not fixtures.is_file():
            raise RuntimeError('Qt emitter did not produce fixtures')
        # Reuse exact project dependency pins. Only the ephemeral module name
        # changes; no repo module or cached dependency is changed/downloaded.
        (root / 'go.mod').write_text('module xhttp-padding-decoder\n' + go_mod.split('\n', 1)[1])
        (root / 'go.sum').write_text(go_sum)
        shutil.copyfile(HERE / 'decoder_test.go', root / 'decoder_test.go')
        env = dict(os.environ, GOPROXY='off', GOSUMDB='off', GOTOOLCHAIN='local', GOWORK='off', CGO_ENABLED='0', XHTTP_FIXTURES=str(fixtures))
        go_result = subprocess.run([os.environ.get('GO', 'go'), 'test', '-mod=readonly', '-count=1', '-v', '.'], cwd=root, env=env, timeout=120)
        if args.artifacts:
            args.artifacts.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(cpp, args.artifacts / cpp.name)
            shutil.copyfile(fixtures, args.artifacts / fixtures.name)
            (args.artifacts / 'source-sha256.txt').write_text(hash_text)
        return 1 if qt_result.returncode or go_result.returncode else 0


if __name__ == '__main__':
    raise SystemExit(main())
