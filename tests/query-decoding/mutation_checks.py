#!/usr/bin/env python3
"""Require baseline pass and runtime assertion failures for every query mutation."""
from pathlib import Path
import re
import subprocess
import sys

from test_query_decoding import MUTATIONS

runner = Path(__file__).with_name('test_query_decoding.py')
for mutation in (None, *MUTATIONS):
    command = [sys.executable, str(runner)]
    if mutation:
        command += ['--mutation', mutation]
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    summary = re.search(r'SUMMARY checks=(\d+) failures=(\d+)', result.stdout)
    expected = result.returncode == (1 if mutation else 0) and summary and ((int(summary[2]) > 0) if mutation else (int(summary[2]) == 0))
    label = mutation or 'unmodified candidate'
    print(label + ': ' + (summary.group(0) if summary else 'NO ASSERTION SUMMARY'), flush=True)
    if not expected:
        print(result.stdout)
        raise SystemExit('Unexpected result for ' + label)
print(f'PASS: baseline and all {len(MUTATIONS)} behavioral mutations')
