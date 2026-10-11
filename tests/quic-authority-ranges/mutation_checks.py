#!/usr/bin/env python3
"""Every mutation must compile and fail a native runtime assertion."""
from pathlib import Path
import subprocess
import sys
from run_test import MUTATIONS

here = Path(__file__).resolve().parent
failed = 0
for mutation in MUTATIONS:
    result = subprocess.run([sys.executable, str(here / "run_test.py"), "--mutation", mutation], capture_output=True, text=True, timeout=90)
    output = result.stdout + result.stderr
    caught = result.returncode == 1 and "RESULT " in output and ", 0 failures" not in output
    print(("CAUGHT " if caught else "MISSED ") + mutation, flush=True)
    if not caught:
        failed += 1
        print(output)
print(f"RESULT {len(MUTATIONS)} mutations, {failed} missed")
raise SystemExit(1 if failed else 0)
