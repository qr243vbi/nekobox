#!/usr/bin/env python3
"""Compile production link handlers and Xray config builder with real Qt6.

Bean ownership and the application data store are test boundaries. No network,
GUI, TUN adapter, user profiles or Xray process is started by this test.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE.parent))
from test_link_tls import function, section


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xray', type=Path, help='Also validate generated synthetic configs with Xray run -test')
    args = parser.parse_args()
    def read(path):
        return (REPO / path).read_text()
    proxy = 'src/gharqad/configs/proxy/'
    stream = read('src/nekobox/configs/proxy/V2RayStreamSettings.hpp')
    fields = section(stream, 'class V2rayStreamSettings : public JsonStore {',
                     'V2rayStreamSettings() : JsonStore()')
    fields += function(stream, 'QMap<QString, QString> GetHeaderPairs(')
    code = read('tests/ech-share-links/support.hpp').replace('STREAM_FIELDS', fields)
    code = '#include <QDir>\n#include <QFile>\n#include <QMap>\n#include <QJsonArray>\n#include <QJsonDocument>\n#include <QJsonObject>\n' + code
    code = code.replace('QString utlsFingerprint = "firefox";',
                        'QString utlsFingerprint = "firefox", log_level = "warning"; bool skip_cert = false;')
    entity = 'struct ProxyEntity { QString name, serverAddress; int serverPort = -1; };'
    replacement = '''struct TrojanVLESSBean;
struct ProxyEntity {
    QString name, serverAddress; int serverPort = -1;
    QString type = "vless";
    std::shared_ptr<Configs::TrojanVLESSBean> storedBean;
    auto TrojanVLESSBean() const { return storedBean; }
};'''
    assert code.count(entity) == 1
    code = code.replace(entity, replacement)
    utils = read('src/gharqad/dataStore/Utils.cpp')
    utils_h = read('src/nekobox/dataStore/Utils.hpp')
    start = utils_h.index('#define GetQuery(url)')
    code += '\n' + utils_h[start:utils_h.index('\n\n', start)] + '\n'
    code += function(utils, 'QString GetQueryValue(') + '\n' + function(utils, 'void AddQueryString(')
    extra = read('src/nekobox/configs/proxy/AbstractBeanExtra.hpp')
    code += '\nnamespace Configs {\nnamespace From_Link {\n'
    code += function(extra, 'void add_network(T * t, const QUrlQuery &obj)').replace(
        'void add_network(', 'template<typename T> void add_network(', 1)
    code += '\n}\n'
    for signature in ('void From_Link::add_tls(', 'void From_Link::add_default_fields(',
                      'void From_Link::add_mux_state('):
        code += function(read(proxy + 'Link2Bean.cpp'), signature) + '\n'
    code += 'namespace To_Link {\n'
    code += function(extra, 'void add_network(QUrlQuery & query, T * obj)').replace(
        'void add_network(', 'template<typename T> void add_network(', 1)
    for signature in ('void add_query_nonempty(', 'void add_default_fields(',
                      'void add_query_boolean(', 'void add_mux_state(', 'void add_tls('):
        code += '\n' + function(read(proxy + 'Bean2Link.cpp'), signature)
    code += '\n}\n'
    for signature in ('bool TrojanVLESSBean::TryParseLink(', 'QString TrojanVLESSBean::ToShareLink() const'):
        code += function(read(proxy + 'TrojanVLESSBean.cpp'), signature) + '\n'
    code += '''const V2rayStreamSettings *GetStreamSettingsConst(const TrojanVLESSBean *bean) {
        return bean ? bean->stream.get() : nullptr;
    }\n'''
    builder = read('src/gharqad/configs/ConfigBuilder.cpp')
    code += function(builder, 'bool IsBuiltinXrayProfile(') + '\n'
    code += function(builder, 'QJsonObject BuildBuiltinXrayConfig(') + '\n}\n'
    updater = read('src/gharqad/configs/sub/GroupUpdater.cpp')
    code += '\nnamespace Subscription {\n'
    for signature in ('static QString jsonFirstString(', 'static int jsonFirstInt(',
                      'static QJsonObject convertV2RayNToSingBox('):
        code += function(updater, signature) + '\n'
    code += '}\n'
    code += (HERE / 'checks.cpp').read_text()
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
    with tempfile.TemporaryDirectory(prefix='nekobox-compat-') as temp:
        cpp, exe = Path(temp) / 'checks.cpp', Path(temp) / 'checks'
        cpp.write_text(code)
        command = shlex.split(os.environ.get('CXX', 'c++'))
        command += ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-O0', '-fPIC',
                    str(cpp), '-o', str(exe)] + flags
        subprocess.run(command, check=True, timeout=90)
        environment = os.environ.copy()
        if args.xray:
            environment['NEKOBOX_TEST_CONFIG_DIR'] = temp
        result = subprocess.run([str(exe)], timeout=10, env=environment)
        if result.returncode or not args.xray:
            return result.returncode
        configs = sorted(Path(temp).glob('fixture-*.json'))
        if not configs:
            raise AssertionError('No synthetic Xray configs emitted')
        for config in configs:
            subprocess.run([str(args.xray.resolve()), 'run', '-test', '-c', str(config)], check=True, timeout=20)
        print(f'XRAY CONFIG VALIDATION: {len(configs)} passed; no connections started')
        return 0


if __name__ == '__main__':
    raise SystemExit(main())
