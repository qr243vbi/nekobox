#!/usr/bin/env python3
"""Require runtime assertion failures for targeted splitter mutations.

Only temporary source copies are changed. Requires the same Qt6 setup as the
regression runner. Compilation errors and timeouts do not count as successes.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SOURCE = Path('src/gharqad/configs/sub/GroupUpdater.cpp')
source = (ROOT / SOURCE).read_text()
mutations = {
    'drop_link_normalization': ('list[0].trimmed() == str)', 'list[0].trimmed() == str.trimmed())'),
    'ignore_string_state': ('    if (inString) {', '    if (false && inString) {'),
    'ignore_escapes': ('        escaped = true;', '        escaped = false;'),
    'sticky_escape': ('      if (escaped)\n        escaped = false;',
                     '      if (escaped)\n        escaped = true;'),
    'never_close_string': ('''      else if (str[i] == '"')
        inString = false;''', '''      else if (str[i] == '"')
        inString = true;'''),
}
failures = 0
for name, (before, after) in mutations.items():
    if source.count(before) != 1:
        raise SystemExit(f'Expected exactly one mutation anchor for {name}')
    with tempfile.TemporaryDirectory(prefix='nekobox-json-splitter-mutation-') as temp:
        repo = Path(temp)
        (repo / SOURCE).parent.mkdir(parents=True)
        (repo / SOURCE).write_text(source.replace(before, after))
        result = subprocess.run([sys.executable, str(HERE / 'test_json_splitter.py'),
                                 '--repo', str(repo), '--splitter-only'],
                                text=True, capture_output=True, timeout=90)
        report = result.stdout + result.stderr
        killed = result.returncode == 1 and 'RESULT:' in report and 'FAIL ' in report
        print(('PASS ' if killed else 'FAIL ') + name, flush=True)
        if not killed:
            failures += 1
            print(report)
print(f'RESULT: {len(mutations) - failures}/{len(mutations)} mutations rejected by runtime assertions')
raise SystemExit(bool(failures))
