#!/usr/bin/env python3
"""Require every mutation to compile, execute, and fail a runtime assertion."""
from pathlib import Path
import subprocess
import sys

from test_detour_chain import MUTATIONS

runner = Path(__file__).with_name('test_detour_chain.py')
for name in MUTATIONS:
    result = subprocess.run([sys.executable, str(runner), '--mutation', name],
                            capture_output=True, text=True, timeout=300)
    if result.returncode != 1 or 'FAIL ' not in result.stdout or 'RESULT ' not in result.stdout:
        print(result.stdout, result.stderr)
        raise SystemExit('Mutation not killed at runtime: ' + name)
    print('KILLED', name, result.stdout.strip().splitlines()[-1])
print('All', len(MUTATIONS), 'mutations killed by runtime checks')
