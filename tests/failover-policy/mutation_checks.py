#!/usr/bin/env python3
"""Verify candidate, known baseline failures, and six behavioral mutations."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
BASELINE = 'a536846c9c74dd8fb104dce3319cbd9672920bb3'
MUTATIONS = (
    'ignore-checkbox', 'gate-proxy-too', 'invert-checkbox', 'ignore-active-id',
    'skip-failure-bookkeeping', 'ignore-setting-value',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=HERE.parents[1])
    parser.add_argument('--qt-root', type=Path)
    args = parser.parse_args()
    base = [sys.executable, str(HERE / 'test_failover_policy.py'), '--repo', str(args.repo)]
    if args.qt_root:
        base += ['--qt-root', str(args.qt_root)]
    cases = [('candidate', [], False), ('baseline', ['--ref', BASELINE], True)]
    cases += [(name, ['--mutation', name], True) for name in MUTATIONS]
    for name, extra, expected_failure in cases:
        result = subprocess.run(base + extra, capture_output=True, text=True, timeout=100)
        combined = result.stdout + result.stderr
        summary = re.search(r'36 scenarios, 377 assertions, (\d+) failures', combined)
        expected_exit = 1 if expected_failure else 0
        valid = (result.returncode == expected_exit and summary is not None and
                 (int(summary.group(1)) > 0) == expected_failure and
                 ('FAIL:' in combined) == expected_failure and
                 'Traceback' not in combined)
        if not valid:
            print(f'UNEXPECTED {name}: exit {result.returncode}\n{combined}')
            return 1
        print(f'{name}: {summary.group(0)}', flush=True)
    print('Candidate passes; baseline and all six mutations fail with assertions.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
