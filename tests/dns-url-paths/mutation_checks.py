#!/usr/bin/env python3
"""Require runtime assertion failures for independent path-preservation faults."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
source = (ROOT / 'src/gharqad/configs/ConfigBuilder.cpp').read_text()
runner = HERE / 'run_test.py'
anchor = 'path = addr.mid(addr.indexOf("/"));'
if source.count(anchor) != 2:
    raise SystemExit('Path extraction anchors changed')
mutations = [
    ('HTTPS last segment', source.replace(anchor, 'path = addr.split("/").last();', 1)),
    ('H3 last segment', source[::-1].replace(anchor[::-1], 'path = addr.split("/").last();'[::-1], 1)[::-1]),
    ('HTTPS removes scheme inside path', source.replace('addr = addr.sliced(8);', 'addr = addr.replace("https://", "");')),
    ('H3 removes scheme inside path', source.replace('addr = addr.sliced(5);', 'addr = addr.replace("h3://", "");')),
    ('drop explicit root', source.replace(anchor, anchor + ' if (path == "/") path = "";')),
    ('drop trailing slash', source.replace(anchor, anchor + ' if (path.endsWith("/")) path.chop(1);')),
    ('decode encoded slash too early', source.replace(anchor, anchor + ' path.replace("%2F", "/").replace("%2f", "/");')),
    ('drop leading slash', source.replace(anchor, 'path = addr.mid(addr.indexOf("/") + 1);')),
]
for label, contents in [('unmodified candidate', source), *mutations]:
    with tempfile.TemporaryDirectory(prefix='nekobox-dns-path-mutant-') as tmp:
        path = Path(tmp) / 'ConfigBuilder.cpp'
        path.write_text(contents)
        result = subprocess.run([sys.executable, str(runner), '--source', str(path)], text=True, capture_output=True, timeout=120)
        summary = re.search(r'SUMMARY Qt checks=(\d+) failures=(\d+)', result.stdout)
        expected_fail = label != 'unmodified candidate'
        if not summary or result.returncode != int(expected_fail) or (int(summary[2]) > 0) != expected_fail:
            print(result.stdout + result.stderr)
            raise SystemExit('Unexpected result: ' + label)
        print(label + ': ' + summary.group(0), flush=True)
print(f'PASS candidate and {len(mutations)} independently caught behavioral mutations')
