#!/usr/bin/env python3
"""Prove the focused regression rejects common broken readiness fixes.

Mutations operate on temporary source copies only. No core is launched.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
CPP = Path('src/gharqad/sys/Process.cpp')
HEADER = Path('src/nekobox/sys/Process.hpp')
source = (ROOT / CPP).read_text(encoding='utf-8')
header = (ROOT / HEADER).read_text(encoding='utf-8')
mutations = [
    ('ignore preceding chunk', 'const auto readinessLog = core_readiness_tail + log;', 'const auto readinessLog = log;'),
    ('forget overlap', 'readinessLog.right(readyMarker.size() - 1)', 'readinessLog.right(0)'),
    ('retain whole marker and duplicate event', 'readinessLog.right(readyMarker.size() - 1)', 'readinessLog.right(readyMarker.size())'),
    ('retain unbounded history', 'readinessLog.right(readyMarker.size() - 1)', 'readinessLog'),
    ('consume remembered profile twice', '                    start_profile_when_core_is_up = -1;', '                    // Incorrectly keep the remembered ID.'),
    ('drop stdout over the display limit from the file log',
     '            if (logCounter.fetchAndAddRelaxed(log.count("\\n")) > maxLogLines()) {\n                LogFile::append(log);\n                return;\n            }\n            MW_show_log(log);\n        });\n        connect(&process, &QProcess::readyReadStandardError',
     '            if (logCounter.fetchAndAddRelaxed(log.count("\\n")) > maxLogLines()) {\n                return;\n            }\n            MW_show_log(log);\n        });\n        connect(&process, &QProcess::readyReadStandardError'),
    ('retain carry across fresh starts', '        core_readiness_tail.clear();', '        // Incorrectly keep the previous run\'s carry.'),
]
failed = 0
for label, before, after in mutations:
    if source.count(before) != 1:
        raise SystemExit(f'Mutation anchor changed: {label}')
    with tempfile.TemporaryDirectory(prefix='nekobox-readiness-mutant-') as tmp:
        root = Path(tmp)
        for path, content in [(CPP, source.replace(before, after)), (HEADER, header)]:
            (root / path).parent.mkdir(parents=True, exist_ok=True)
            (root / path).write_text(content, encoding='utf-8')
        result = subprocess.run([sys.executable, str(HERE / 'run_test.py'), '--repo', str(root)], text=True, capture_output=True, timeout=90)
        output = result.stdout + result.stderr
        expected = ('Readiness carry must reset' if label == 'retain carry across fresh starts' else 'SUMMARY assertions=')
        caught = result.returncode != 0 and expected in output and ('failures=0' not in output)
        print(('CAUGHT ' if caught else 'MISSED ') + label, flush=True)
        if not caught:
            failed += 1
            print(output)
print(f'SUMMARY mutations={len(mutations)} missed={failed}')
raise SystemExit(1 if failed else 0)
