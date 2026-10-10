#!/usr/bin/env python3
"""Offline regression for CoreProcess's restart mutex ownership.

Run: python3 tests/test_process_restart_lock.py [--repo PATH] [--ref COMMIT]
Or:  python3 -m unittest discover -s tests -v
Requires Python 3 and a GCC/Clang-compatible C++20 driver (CXX, or g++).
CXX is parsed as shell-style words; an MSVC command line is not supported.

The stateChanged connection and Restart() definition are compiled verbatim from
Process.cpp. The companion fixture substitutes Qt/process/GUI/timer boundaries;
it never starts a core, invokes privilege tools, or changes network settings.
This is a deterministic source-linked check, not a full Qt integration test.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
REF = None
SOURCE_PATH = "src/gharqad/sys/Process.cpp"


def extract(source, start, end):
    if source.count(start) != 1:
        raise ValueError(f"Expected one source anchor: {start!r}")
    begin = source.index(start)
    finish = source.index(end, begin) + len(end)
    return source[begin:finish]


class ProcessRestartLockTest(unittest.TestCase):
    def test_restart_lock_paths(self):
        if REF:
            source = subprocess.check_output(
                ["git", "-C", str(REPO), "show", f"{REF}:{SOURCE_PATH}"],
                text=True,
            )
        else:
            source = (REPO / SOURCE_PATH).read_text(encoding="utf-8")
        connection = extract(
            source,
            "        connect(&process, &QProcess::stateChanged, this, "
            "[&](QProcess::ProcessState state) {",
            "\n        });",
        )
        restart = extract(source, "    void CoreProcess::Restart() {", "\n    }")
        compiled_source = (
            "namespace Configs_sys {\nCoreProcess::CoreProcess() {\n"
            + connection + "\n}\n" + restart + "\n}\n"
        )
        fixture = Path(__file__).with_name("process_restart_lock_fixture.cpp")
        with tempfile.TemporaryDirectory(prefix="nekobox-process-restart-") as tmp:
            tmp = Path(tmp)
            (tmp / "process_restart_source.inc").write_text(
                compiled_source, encoding="utf-8"
            )
            binary = tmp / ("restart-test.exe" if os.name == "nt" else "restart-test")
            compiler = shlex.split(os.environ.get("CXX", "g++"))
            command = compiler + [
                "-std=c++20", "-Wall", "-Wextra", "-Werror", "-O0",
                "-I", str(tmp), str(fixture), "-o", str(binary),
            ]
            print(f"SOURCE {REF or 'working tree'} SHA256 "
                  f"{hashlib.sha256(source.encode()).hexdigest()}", flush=True)
            compiled = subprocess.run(command, text=True, capture_output=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=10)
            print(result.stdout, end="", flush=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=REPO)
    parser.add_argument("--ref", help="Read source from this Git revision")
    args = parser.parse_args()
    REPO = args.repo.resolve()
    REF = args.ref
    unittest.main(argv=[__file__], verbosity=2)
