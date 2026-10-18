#!/usr/bin/env python3
"""Import of sing-box outbounds linked by "detour" into hop profiles plus a chain.

Runs the production importer, chain resolver and profile key code, extracted
from the source files, against Qt6 Core and in-memory profile storage.
"""
import argparse
import hashlib
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from test_link_tls import function, section  # noqa: E402

UPDATER = 'src/gharqad/configs/sub/GroupUpdater.cpp'
UPDATER_H = 'src/nekobox/configs/sub/GroupUpdater.hpp'
FILTER = 'src/gharqad/dataStore/ProfileFilter.cpp'
FILTER_H = 'src/nekobox/dataStore/ProfileFilter.hpp'
BUILDER = 'src/gharqad/configs/ConfigBuilder.cpp'

# (file, before, after): every one must make a runtime check fail
MUTATIONS = {
    'hops-in-wrong-order': (UPDATER, 'path.prepend(cur);', 'path.append(cur);'),
    'hop-not-marked': (UPDATER, 'hop.ent->chain_link = true;', 'hop.ent->chain_link = false;'),
    'main-not-first': (UPDATER, 'const int main_exit = exits.first();', 'const int main_exit = exits.last();'),
    'chains-cannot-be-switched-off': (
        UPDATER, 'if (allow_chains && updateSingBoxChains(items, json, name)) {',
        'if (updateSingBoxChains(items, json, name)) {'),
    'chain-not-moved-up': (UPDATER, 'if (positions.size() == want.size()) {', 'if (false) {'),
    'chain-duplicated-on-update': (UPDATER, 'if (!AddProxy(chain.ent)) continue;', 'AddProxy(chain.ent);'),
    'chain-id-not-kept': (
        UPDATER, 'if (Configs::profileManager->ReplaceProfile(old->id, chain.ent)) {', 'if (false) {'),
    'shared-hop-stored-twice': (UPDATER, 'if (hop_done.contains(i)) continue;', ''),
    'hops-keyed-by-address': (FILTER, ' &&\n                                ent->type != "chain" && !ent->chain_link;', ';'),
    'reused-profile-keeps-old-role': (UPDATER, 'old->chain_link = hop;', '(void)hop;'),
    'direct-detour-logged': (
        UPDATER, 'if (detour != "direct") {', 'if (false) {'),
}


def compile_command(cpp, exe, defines=()):
    cxx = os.environ.get('CXX')
    qt_dir = os.environ.get('QT_DIR')
    if (cxx and Path(cxx).name.lower().startswith('cl')) or (not cxx and qt_dir and not shutil.which('pkg-config')):
        # MSVC, from a developer prompt; QT_DIR is the Qt kit root
        qt = Path(qt_dir)
        return [cxx or 'cl', '/nologo', '/std:c++20', '/EHsc', '/permissive-', '/Zc:__cplusplus', '/utf-8',
                '/W3', '/wd4244', '/wd4267', '/DUSE_REAL_QT', *[f'/D{d}' for d in defines], f'/I{qt / "include"}', f'/I{qt / "include" / "QtCore"}',
                str(cpp), f'/Fe{exe}', f'/Fo{Path(cpp).parent}\\', '/link', f'/LIBPATH:{qt / "lib"}', 'Qt6Core.lib']
    probe = subprocess.run(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True, capture_output=True)
    if probe.returncode:
        raise SystemExit('BLOCKED: Qt6 Core development headers/pkg-config metadata unavailable.')
    return (shlex.split(cxx or 'c++') + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-unused-variable',
            '-Wno-unused-function', '-O0', '-fPIC', '-DUSE_REAL_QT'] + [f'-D{d}' for d in defines] + shlex.split(os.environ.get('EXTRA_CXXFLAGS', ''))
            + [str(cpp), '-o', str(exe)] + shlex.split(probe.stdout))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref', help='Read production source from an immutable Git revision')
    parser.add_argument('--mutation', choices=MUTATIONS)
    parser.add_argument('--keep-generated', type=Path)
    args = parser.parse_args()
    hashes = {}

    def read(path):
        text = (subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'], text=True)
                if args.ref else (args.repo / path).read_text(encoding='utf-8'))
        if args.mutation:
            target, before, after = MUTATIONS[args.mutation]
            if path == target:
                if text.count(before) != 1:
                    raise ValueError('Mutation anchor changed: ' + args.mutation)
                text = text.replace(before, after)
        hashes[path] = hashlib.sha256(text.encode()).hexdigest()
        return text

    updater, updater_h = read(UPDATER), read(UPDATER_H)
    flt, flt_h, builder = read(FILTER), read(FILTER_H), read(BUILDER)

    key_decl = section(flt_h, 'struct ProfileFilterKey\n{', '// --- Hash ---')
    key_decl = 'struct ProfileFilterKey\n{' + key_decl
    # the header also declares the factory used by the key tests
    key_decl += '\nProfileFilterKey ProfileFilter_ent_key(const std::shared_ptr<Configs::ProxyEntity> &ent, bool by_address);\n'
    raw_decl = '    class RawUpdater {' + section(updater_h, '    class RawUpdater {', '    class GroupUpdater')

    code = (HERE / 'support.hpp').read_text()
    code = code.replace('PROFILE_FILTER_KEY_DECL', key_decl).replace('RAW_UPDATER_DECL', raw_decl)

    # profile key bodies
    code += '\nnamespace Configs {\n'
    for sig in ('bool ProfileFilterKey::operator==(', 'bool ProfileFilterKey::operator!=(',
                'bool ProfileFilterKey::operator<(', 'ProfileFilterKey ProfileFilter_ent_key('):
        code += function(flt, sig) + '\n'
    code += function(flt, 'ProfileFilterKey::ProfileFilterKey(const std::shared_ptr<Configs::ProxyEntity>& key,') + '\n'
    code += function(builder, 'ResolveChainInternal(const std::shared_ptr<BuildConfigStatus> &status,').replace(
        'ResolveChainInternal(', 'QList<std::shared_ptr<ProxyEntity>>\nResolveChainInternal(', 1) + '\n}\n'

    # the importer: helpers, updateSingBox, ProfileId, FinalizeChains and AddProxy
    start = updater.index('namespace {\n\n// One outbound of a sing-box json')
    end = updater.index('bool RawUpdater::updateWireguardFileConfig(')
    code += '\nnamespace Subscription {\n' + updater[start:end]
    code += function(updater, 'bool RawUpdater::AddProxy(') + '\n}\n'
    code += (HERE / 'checks.cpp').read_text()

    if args.keep_generated:
        args.keep_generated.write_text(code)
    print('SOURCE:', args.ref or 'working tree', flush=True)
    for path, digest in hashes.items():
        print(' ', digest, path, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-detour-chain-') as td:
        cpp, exe = Path(td) / 'test.cpp', Path(td) / ('test.exe' if os.name == 'nt' else 'test')
        cpp.write_text(code)
        subprocess.run(compile_command(cpp, exe), check=True, timeout=240, cwd=td)
        return subprocess.run([str(exe)], timeout=30).returncode


if __name__ == '__main__':
    raise SystemExit(main())
