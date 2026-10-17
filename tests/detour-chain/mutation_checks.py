#!/usr/bin/env python3
"""Require every mutation to compile, execute, and fail a runtime assertion."""
from pathlib import Path
import subprocess
import sys

import test_detour_chain
import test_import_compat

total = 0
for module in (test_detour_chain, test_import_compat):
    runner = Path(module.__file__)
    for name in module.MUTATIONS:
        result = subprocess.run([sys.executable, str(runner), '--mutation', name],
                                capture_output=True, text=True, timeout=300)
        if result.returncode != 1 or 'FAIL ' not in result.stdout or 'RESULT ' not in result.stdout:
            print(result.stdout, result.stderr)
            raise SystemExit('Mutation not killed at runtime: ' + name)
        print('KILLED', runner.name, name, result.stdout.strip().splitlines()[-1])
        total += 1
print('All', total, 'mutations killed by runtime checks')
