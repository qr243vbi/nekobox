#!/usr/bin/env python3
"""Require real assertion failures for independent emitter regressions."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from test_xhttp_padding import PRODUCTION

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
CORE = 'src/gharqad/configs/proxy/Bean2CoreObj_box.cpp'
GUARD = 'if (!transport.contains("x_padding_bytes")) {'
BLOCK = '''                if (!transport.contains("x_padding_bytes")) {
                    transport["x_padding_bytes"] = QJsonObject{{"from", 100}, {"to", 1000}};
                }'''

mutations = [
    ('drop default', BLOCK, '                /* omitted default */'),
    ('wrong lower endpoint', 'QJsonObject{{"from", 100}, {"to", 1000}}', 'QJsonObject{{"from", 10}, {"to", 1000}}'),
    ('wrong upper endpoint', 'QJsonObject{{"from", 100}, {"to", 1000}}', 'QJsonObject{{"from", 100}, {"to", 100}}'),
    ('overwrite explicit padding', GUARD, 'if (true) {'),
    ('silently repair invalid padding', GUARD, 'if (!transport.contains("x_padding_bytes") || transport["x_padding_bytes"].toObject()["from"].toInt() <= 0) {'),
    ('default only auto mode', GUARD, 'if (xhttp_mode == "auto" && !transport.contains("x_padding_bytes")) {'),
    ('discard explicit extra', 'parseExtraXhttp(transport, xhttp_extra);', '/* discard extra */'),
    ('leak default into other transports', 'QJsonObject transport{{"type", network}};', 'QJsonObject transport{{"type", network}, {"x_padding_bytes", QJsonObject{{"from", 100}, {"to", 1000}}}};'),
]

for name, old, new in mutations:
    with tempfile.TemporaryDirectory(prefix='nekobox-xhttp-padding-mutation-') as temp:
        repo = Path(temp)
        for rel in PRODUCTION:
            target = repo / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO / rel, target)
        target = repo / CORE
        source = target.read_text()
        assert source.count(old) == 1, name
        target.write_text(source.replace(old, new))
        result = subprocess.run([sys.executable, str(HERE / 'test_xhttp_padding.py'), '--repo', str(repo)], capture_output=True, text=True, timeout=180)
        summaries = [line for line in result.stdout.splitlines() if line.startswith('SUMMARY')]
        # A compile/dependency failure cannot count as a caught mutation.
        if result.returncode != 1 or len(summaries) != 1 or 'failures=0 ' in summaries[0] or 'FAIL ' not in result.stdout or 'Traceback' in result.stderr:
            print(result.stdout, result.stderr)
            raise SystemExit('Mutation not detected by assertions: ' + name)
        print('CAUGHT', name, summaries[0], flush=True)
print('PASS: all eight production mutations detected')
