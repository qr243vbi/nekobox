#!/usr/bin/env python3
"""Reject isolated Remember slot mutations using temp-INI round trips."""
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
paths = ('src/gharqad/ui/mainwindow.cpp', 'src/gharqad/sys/Settings.cpp',
         'src/nekobox/sys/Settings.h', 'src/gharqad/dataStore/Configs.cpp')
sources = {path: (ROOT / path).read_text() for path in paths}
main = sources[paths[0]]
old = '''            Configs::windowSettings->remember_last_profile = checked;
            ui->actionRemember_last_proxy->setChecked(checked);
            Configs::windowSettings->Save();'''
mutations = {
    'wrong store': old.replace('Configs::windowSettings->Save()', 'Configs::dataStore->Save()'),
    'omit save': old.replace('Configs::windowSettings->Save();', ''),
    'invert requested value': old.replace('remember_last_profile = checked', 'remember_last_profile = !checked'),
}
if main.count(old) != 1:
    raise SystemExit('Remember slot anchor changed')
for name, new in mutations.items():
    with tempfile.TemporaryDirectory(prefix='nekobox-remember-mutation-') as temp:
        for path, source in sources.items():
            target = Path(temp) / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(source.replace(old, new) if path == paths[0] else source)
        result = subprocess.run([sys.executable, str(HERE / 'test_remember.py'), '--repo', temp], text=True, capture_output=True, timeout=90)
    if result.returncode != 1 or 'FAIL ' not in result.stdout or 'SUMMARY ' not in result.stdout or result.stderr:
        print(result.stdout, result.stderr)
        raise SystemExit('Mutation not rejected by runtime assertions: ' + name)
    print('PASS rejected:', name, '|', result.stdout.splitlines()[-1])
print(f'SUMMARY {len(mutations)}/{len(mutations)} mutations rejected')
