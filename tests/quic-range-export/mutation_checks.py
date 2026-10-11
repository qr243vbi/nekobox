#!/usr/bin/env python3
"""Every mutation must compile, then fail runtime assertions."""
import subprocess
import sys
from pathlib import Path
from run_test import MUTATIONS

runner = Path(__file__).with_name('run_test.py')
missed = 0
for name in MUTATIONS:
    result = subprocess.run([sys.executable, str(runner), '--mutation', name], text=True, capture_output=True)
    lines = [line for line in result.stdout.splitlines() if line.startswith('RESULT ')]
    killed = result.returncode == 1 and len(lines) == 1 and not lines[0].endswith(', 0 failures')
    print(name, 'KILLED' if killed else 'MISSED OR ERROR', lines[-1] if lines else '', flush=True)
    if not killed:
        missed += 1
        print(result.stdout, result.stderr)
print(f'RESULT {len(MUTATIONS)} mutations, {missed} missed')
raise SystemExit(bool(missed))
