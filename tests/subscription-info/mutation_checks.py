#!/usr/bin/env python3
"""Prove targeted subscription-display regressions reject five broken variants."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = 'src/gharqad/ui/group/GroupItem.cpp'
PATHS = (SOURCE, 'src/gharqad/dataStore/Utils.cpp', 'src/nekobox/dataStore/Configs.hpp',
         'src/gharqad/dataStore/ConfigItem.cpp', 'src/gharqad/dataStore/HTTPRequestHelper.cpp')
source = (ROOT / SOURCE).read_text()
mutations = {
    'restore_total_gate': ('    const auto upload = readField("upload");',
                          '    if (!info.contains(QRegularExpression("total=([0-9]+)"))) return "";\n    const auto upload = readField("upload");'),
    'show_epoch_for_missing_expiry': ('    QString expiryText = "-";',
                                    '    QString expiryText = DisplayTime(0, QLocale::ShortFormat);'),
    'conflate_missing_total_with_unlimited': ('    QString remainingText = "-";',
                                             '    QString remainingText = QString::fromUtf8("\\u221E");'),
    'remove_sum_guard': ('&& upload.value <= maximum - download.value;', ';'),
    'remove_timestamp_guard': (' && expire.value <= maximum / 1000', ''),
}
for name, (before, after) in mutations.items():
    if source.count(before) != 1:
        raise SystemExit(f'Changed mutation anchor: {name}')
    with tempfile.TemporaryDirectory(prefix='nekobox-subinfo-mutation-') as directory:
        repo = Path(directory)
        for path in PATHS:
            out = repo / path
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(source.replace(before, after) if path == SOURCE else (ROOT / path).read_text())
        result = subprocess.run([sys.executable, str(ROOT / 'tests/test_subscription_info.py'), '--repo', str(repo)],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        if result.returncode != 1 or 'SUMMARY ' not in result.stdout:
            print(result.stdout)
            raise SystemExit(f'Mutation did not fail through behavioral checks: {name} exit={result.returncode}')
        print(f'PASS killed {name}: {result.stdout.splitlines()[-1]}', flush=True)
print('SUMMARY five mutations rejected by runtime checks')
