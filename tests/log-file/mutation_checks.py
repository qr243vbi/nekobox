#!/usr/bin/env python3
"""Require every mutation of LogFile.hpp to compile and fail a test."""
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1] / "src/nekobox/global/LogFile.hpp"
source = SOURCE.read_text(encoding="utf-8")
MUTATIONS = {
    "colour-codes-kept": (r"sanitizeLog(text).split('\n')", r"text.split('\n')"),
    "path-follows-working-directory": ("QFileInfo(env).absoluteFilePath()", "env"),
    "previous-handler-dropped": ("          previous(type, context, msg);", ""),
    "blank-lines-kept": ("if (!line.trimmed().isEmpty()) {", "if (true) {"),
    "qt-messages-unlabelled": ('append(QString::fromLatin1(name) + msg);', 'append(msg);'),
}
failed = 0
for name, (before, after) in MUTATIONS.items():
    if source.count(before) != 1:
        raise SystemExit("Mutation anchor changed: " + name)
    with tempfile.TemporaryDirectory(prefix="nekobox-log-file-mutant-") as tmp:
        target = Path(tmp) / "nekobox" / "global" / "LogFile.hpp"
        target.parent.mkdir(parents=True)
        target.write_text(source.replace(before, after), encoding="utf-8")
        env = dict(os.environ, LOG_FILE_TEST_INCLUDE=tmp)
        result = subprocess.run([sys.executable, str(HERE / "test_log_file.py")], env=env,
                                capture_output=True, text=True, timeout=600)
        output = result.stdout + result.stderr
        # a failed or erroring test case; a mutant that does not build is not killed
        killed = (result.returncode != 0 and ("FAIL: test_" in output or "ERROR: test_" in output)
                  and "ERROR: setUpClass" not in output)
        print(("KILLED " if killed else "MISSED ") + name, flush=True)
        if not killed:
            print(output[-2000:])
            failed += 1
if failed:
    raise SystemExit(f"{failed} mutation(s) not killed by a test failure")
print("All", len(MUTATIONS), "mutations killed")
