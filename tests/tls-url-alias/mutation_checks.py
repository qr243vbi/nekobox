#!/usr/bin/env python3
"""Every mutant must compile and fail runtime assertions; errors are not kills."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

HERE = Path(__file__).resolve().parent
from test_tls_url_alias import MUTATIONS

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=HERE.parents[1])
parser.add_argument('--logs', type=Path)
args = parser.parse_args()
if args.logs:
    args.logs.mkdir(parents=True, exist_ok=True)
count = 0
for mutation in MUTATIONS:
    shared = mutation in ('decode-before-split', 'omit-json-flag', 'omit-export-flag')
    for target in (('both',) if shared else ('trojan-vless', 'vmess')):
        command = [sys.executable, str(HERE / 'test_tls_url_alias.py'), '--repo', str(args.repo),
                   '--mutation', mutation, '--mutation-target', target]
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        if args.logs:
            (args.logs / (mutation + '-' + target + '.log')).write_text(output)
        summary = re.search(r'TLS URL alias checks: (\d+) checks, (\d+) failures', output)
        if result.returncode != 1 or not summary or int(summary.group(2)) == 0:
            print(output)
            raise SystemExit('NOT KILLED at runtime: ' + mutation + '/' + target)
        count += 1
        print('KILLED ' + mutation + '/' + target + ': ' + summary.group(), flush=True)
print(f'Mutation controls: {count}/{count} compiled and killed at runtime')
