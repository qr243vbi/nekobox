#!/usr/bin/env python3
"""Imports that existed before chain import still give the same profiles.

Runs RawUpdater::update() with the production base64 decoder, splitter,
full-config sanitizer and sing-box/SIP008 importers, extracted from the
sources, against Qt6 Core and in-memory profile storage. The same inputs and
expectations run against any revision: use --ref to check one without chain
import, e.g. --ref a83f27fc.
"""
import argparse
import hashlib
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))
from test_link_tls import function, section  # noqa: E402
from test_detour_chain import compile_command  # noqa: E402

UPDATER = 'src/gharqad/configs/sub/GroupUpdater.cpp'
UPDATER_H = 'src/nekobox/configs/sub/GroupUpdater.hpp'
FILTER = 'src/gharqad/dataStore/ProfileFilter.cpp'
FILTER_H = 'src/nekobox/dataStore/ProfileFilter.hpp'
UTILS = 'src/gharqad/dataStore/Utils.cpp'
UTILS_H = 'src/nekobox/dataStore/Utils.hpp'
BASE64 = 'src/gharqad/dataStore/base64.cpp'

# (file, before, after): every one must make a runtime check fail
MUTATIONS = {
    'full-config-not-detected': (
        UPDATER, '''      if (json_contains_inbounds || json.contains("routing") ||
          json.contains("route") || json.contains("dns")) {''', '''      if (json_contains_inbounds) {'''),
    'base64-not-decoded': (UPDATER, 'int i = 32;', 'int i = 0;'),
    'multiline-not-split': (UPDATER, r'if (str.count("\n") > 0) {', 'if (false) {'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--ref', help='Read production source from an immutable Git revision')
    parser.add_argument('--mutation', choices=MUTATIONS)
    parser.add_argument('--keep-generated', type=Path)
    args = parser.parse_args()
    hashes = {}

    def read(path):
        text = (subprocess.check_output(['git', '-C', str(args.repo), 'show', f'{args.ref}:{path}'],
                                        text=True, encoding='utf-8')
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
    flt, flt_h = read(FILTER), read(FILTER_H)
    utils, utils_h, b64 = read(UTILS), read(UTILS_H), read(BASE64)
    has_chains = 'void RawUpdater::FinalizeChains(' in updater

    key_decl = 'struct ProfileFilterKey\n{' + section(flt_h, 'struct ProfileFilterKey\n{', '// --- Hash ---')
    key_decl += '\nProfileFilterKey ProfileFilter_ent_key(const std::shared_ptr<Configs::ProxyEntity> &ent, bool by_address);\n'
    raw_decl = '    class RawUpdater {' + section(updater_h, '    class RawUpdater {', '    class GroupUpdater')

    code = '#define REAL_IMPORT_HELPERS\n' + (HERE / 'support.hpp').read_text(encoding='utf-8')
    code = code.replace('PROFILE_FILTER_KEY_DECL', key_decl).replace('RAW_UPDATER_DECL', raw_decl)

    # base64 decoding exactly as the application does it
    code += '\nnamespace QtBase64 {' + section(utils_h, 'namespace QtBase64 {', 'namespace Qt515Base64 = QtBase64;')
    code += '\nnamespace Qt515Base64 = QtBase64;\n'
    code += b64.replace('#include <nekobox/dataStore/Utils.hpp>', '') + '\n'
    # base64.cpp is its own translation unit in the application; its
    # fallback "#define qsizetype size_t" must not reach the importer here
    code += '#undef qsizetype\n'
    code += function(utils, 'QByteArray DecodeB64IfValid(') + '\n'

    code += '\nnamespace Configs {\n'
    for sig in ('bool ProfileFilterKey::operator==(', 'bool ProfileFilterKey::operator!=(',
                'bool ProfileFilterKey::operator<(', 'ProfileFilterKey ProfileFilter_ent_key('):
        code += function(flt, sig) + '\n'
    code += function(flt, 'ProfileFilterKey::ProfileFilterKey(const std::shared_ptr<Configs::ProxyEntity>& key,') + '\n}\n'

    # splitter, helpers, sanitizer, update(), SIP008 and sing-box import (with
    # chain import where the revision has it), then AddProxy
    start = updater.index('int JsonEndIdx(')
    end = updater.index('bool RawUpdater::updateWireguardFileConfig(')
    code += '\nnamespace Subscription {\n' + updater[start:end]
    code += function(updater, 'bool RawUpdater::AddProxy(') + '\n'
    # Clash and WireGuard files are not inputs of these cases
    code += 'bool RawUpdater::updateClash(const QString &) { return false; }\n'
    code += 'bool RawUpdater::updateWireguardFileConfig(const QString &) { return false; }\n}\n'
    code += (HERE / 'compat_checks.cpp').read_text(encoding='utf-8')

    if args.keep_generated:
        args.keep_generated.write_text(code, encoding='utf-8')
    print('SOURCE:', args.ref or 'working tree', '| chain import:', 'yes' if has_chains else 'no', flush=True)
    for path, digest in hashes.items():
        print(' ', digest, path, flush=True)
    with tempfile.TemporaryDirectory(prefix='nekobox-import-compat-') as td:
        cpp, exe = Path(td) / 'test.cpp', Path(td) / ('test.exe' if os.name == 'nt' else 'test')
        cpp.write_text(code, encoding='utf-8')
        defines = ['HAS_CHAINS'] if has_chains else []
        subprocess.run(compile_command(cpp, exe, defines), check=True, timeout=240, cwd=td)
        return subprocess.run([str(exe)], timeout=30).returncode


if __name__ == '__main__':
    raise SystemExit(main())
