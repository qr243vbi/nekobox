#!/usr/bin/env python3
"""Offline production Clash updater -> stream -> sing-box JSON regression.

Uses actual yaml-cpp and Qt6. See README for the isolated factory/storage edges.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
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
    'src/gharqad/configs/sub/GroupUpdater.cpp',
    'src/gharqad/configs/proxy/Bean2CoreObj_box.cpp',
    'src/nekobox/configs/proxy/AbstractBeanExtra.hpp',
    'src/gharqad/dataStore/base64.cpp',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref')
    parser.add_argument('--keep-generated', type=Path)
    args = parser.parse_args()
    hashes = {}
    def read(path):
        source = subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True) if args.ref else (args.repo / path).read_text()
        hashes[path] = hashlib.sha256(source.encode()).hexdigest()
        return source
    utils, utils_h, stream, updater, core, extra, base64 = map(read, PRODUCTION)
    fields = section(stream, 'class V2rayStreamSettings : public JsonStore {', 'V2rayStreamSettings() : JsonStore()')
    kcp = section(stream, 'class KCPExtra: public JsonStore {', 'KCPExtra(){};')
    code = (HERE / 'support.hpp').read_text().replace('STREAM_FIELDS', fields).replace('KCP_FIELDS', kcp).replace('HEADER_PAIRS', function(stream, 'QMap<QString, QString> GetHeaderPairs('))
    code += '\n' + section(utils_h, '#include <utility>\n', '\n\nQString Quote(')
    for signature in ('QString Node2QString(', 'QStringList Node2QStringList(', 'int Node2Int(', 'bool Node2Bool(', 'YAML::Node NodeChild(', 'QString SubStrBefore(', 'QString SubStrAfter(', 'QJsonArray QListStr2QJsonArray(', 'QJsonObject QMapString2QJsonObject('):
        code += '\n' + function(utils, signature)
    if 'static bool ParseClashEch(' in updater:
        begin = utils_h.index('namespace QtBase64 {')
        code += utils_h[begin:utils_h.index('namespace Qt515Base64 = QtBase64;', begin) + len('namespace Qt515Base64 = QtBase64;')]
        code += '\n' + base64[base64.index('#ifndef qsizetype'):]
        code += '\n' + function(utils, 'QByteArray DecodeB64IfValid(')
    code += '\nnamespace Subscription {\n'
    if 'static bool ParseClashEch(' in updater:
        code += function(updater, 'static bool ParseClashEch(') + '\n'
    code += function(updater, 'void RawUpdater_FixEnt(') + '\n'
    code += function(updater, 'bool RawUpdater::updateClash(') + '\n}\n'
    code += '\nnamespace Configs { namespace CoreObj_box {\n'
    code += 'QJsonObject getXbadoptionRange(const QJsonValue &);\n'
    code += 'QJsonObject getXmux(const QJsonValue &);\nint domainStrategy(const QJsonValue &);\n'
    code += 'void parseExtraXhttp(QJsonObject &, QString);\nvoid parseExtraKCP(QJsonObject &, std::shared_ptr<KCPExtra>);\n'
    code += 'template<typename T>\n' + function(extra, 'inline void add_non_empty(T & obj, const QString & key, const QString & value)') + '\n}\n'
    for signature in ('QJsonObject CoreObj_box::getXmux(', 'int CoreObj_box::domainStrategy(', 'QJsonObject CoreObj_box::getXbadoptionRange(', 'void CoreObj_box::parseExtraKCP(', 'void CoreObj_box::parseExtraXhttp('):
        code += function(core, signature) + '\n'
    code += 'using namespace CoreObj_box;\n' + function(core, 'void V2rayStreamSettings::BuildStreamSettingsSingBox(') + '\n}\n'
    code += (HERE / 'checks.cpp').read_text()
    if args.keep_generated:
        args.keep_generated.write_text(code)
    qt_flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    root = os.environ.get('YAML_CPP_ROOT')
    yaml_flags = ['-I' + str(Path(root) / 'usr/include'), str(Path(root) / 'usr/lib/x86_64-linux-gnu/libyaml-cpp.a')] if root else shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'yaml-cpp'], text=True))
    print('Source:', args.ref or 'working tree', '| real Qt6 and yaml-cpp', flush=True)
    for path, digest in hashes.items(): print(digest, path, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-clash-xhttp-') as temp:
        cpp = Path(temp) / 'test.cpp'; exe = Path(temp) / 'test'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC']
        command += shlex.split(os.environ.get('EXTRA_CXXFLAGS', '')) + [str(cpp), '-o', str(exe)] + qt_flags + yaml_flags
        subprocess.run(command, check=True, timeout=90)
        return subprocess.run([str(exe)], timeout=10).returncode


if __name__ == '__main__': raise SystemExit(main())
