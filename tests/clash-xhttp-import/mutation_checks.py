#!/usr/bin/env python3
"""Ensure independent production import/export regressions are caught."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from test_clash_xhttp_import import PRODUCTION
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
UPDATER = 'src/gharqad/configs/sub/GroupUpdater.cpp'
CORE = 'src/gharqad/configs/proxy/Bean2CoreObj_box.cpp'
mutations = [
    ('drop path', UPDATER, 'bean->stream->path = Node2QString(xhttp["path"]);', 'bean->stream->path = "";'),
    ('drop host', UPDATER, 'bean->stream->host = Node2QString(xhttp["host"]);', 'bean->stream->host = "";'),
    ('drop mode', UPDATER, 'Node2QString(xhttp["mode"], bean->stream->xhttp_mode);', 'QString("auto");'),
    ('lose absent-mode default', UPDATER, 'Node2QString(xhttp["mode"], bean->stream->xhttp_mode);', 'Node2QString(xhttp["mode"]);'),
    ('ignore network guard', UPDATER, 'type == "vless" && static_cast<QString>(*bean->stream->network) == "xhttp"', 'type == "vless"'),
    ('ignore protocol guard', UPDATER, 'type == "vless" && static_cast<QString>(*bean->stream->network) == "xhttp"', 'static_cast<QString>(*bean->stream->network) == "xhttp"'),
    ('drop JSON mode output', CORE, 'transport["mode"] = xhttp_mode;', '/* dropped mode */'),
    ('restore invalid enum comparison', UPDATER, 'static_cast<QString>(*bean->stream->network) == "xhttp"', '*bean->stream->network == "xhttp"'),
]
for name, path, old, new in mutations:
    with tempfile.TemporaryDirectory(prefix='nekobox-clash-xhttp-mutation-') as temp:
        repo = Path(temp)
        for rel in PRODUCTION:
            target = repo / rel; target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO / rel, target)
        target = repo / path; source = target.read_text()
        assert source.count(old) == 1, name
        target.write_text(source.replace(old, new))
        result = subprocess.run([sys.executable, str(HERE / 'test_clash_xhttp_import.py'), '--repo', str(repo)], capture_output=True, text=True, timeout=120)
        summaries = [line for line in result.stdout.splitlines() if line.startswith('SUMMARY')]
        if name == 'restore invalid enum comparison':
            if result.returncode != 1 or summaries or not all(token in result.stderr for token in ('error:', 'operator==', 'V2RAYTransportsEnum', '*bean->stream->network == "xhttp"')):
                print(result.stdout, result.stderr)
                raise SystemExit('Invalid comparison did not fail at the actual enum: ' + name)
            print('CAUGHT', name, 'production enum compile error', flush=True)
            continue
        if result.returncode != 1 or not summaries or 'failures=0' in summaries[-1] or 'Traceback' in result.stderr:
            print(result.stdout, result.stderr)
            raise SystemExit('Mutation not detected by assertions: ' + name)
        print('CAUGHT', name, summaries[-1], flush=True)
print('PASS: seven behavior mutations and one actual-type compile regression detected')
