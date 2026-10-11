#!/usr/bin/env python3
"""Require each mutation to compile and reach a failing runtime assertion."""
from pathlib import Path
import re
import subprocess
import sys

from test_manual_tls import MUTATIONS

runner = Path(__file__).with_name('test_manual_tls.py')
for mutation in MUTATIONS:
    result = subprocess.run([sys.executable, str(runner), '--mutation', mutation],
                            text=True, capture_output=True, timeout=120)
    summary = re.search(r'^SUMMARY checks=(\d+) failures=(\d+)$', result.stdout, re.MULTILINE)
    if result.returncode != 1 or summary is None or int(summary.group(2)) == 0:
        print(result.stdout, result.stderr)
        raise SystemExit('INVALID OR SURVIVING MUTATION: ' + mutation)
    print('KILLED', mutation, summary.group(0), flush=True)
print('SUMMARY mutations=' + str(len(MUTATIONS)) + ' killed=' + str(len(MUTATIONS)))
