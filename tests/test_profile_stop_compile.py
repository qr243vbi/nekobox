#!/usr/bin/env python3
"""Compile the exact profile_stop definition without linking or running it.

Run: python3 tests/test_profile_stop_compile.py [--repo PATH] [--ref COMMIT]
Requires Python 3 and a GCC/Clang-compatible C++20 driver (CXX, or g++).
The companion stubs are only synthetic declarations for external types. This
checks local C++ syntax and source snapshot binding, not a full application build
or Qt/runtime behavior. No profile, worker, core, network, or VPN is started.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]
REF = None
SOURCE_PATH = "src/gharqad/ui/mainwindow_rpc.cpp"


def read_function():
    if REF:
        source = subprocess.check_output(
            ["git", "-C", str(REPO), "show", f"{REF}:{SOURCE_PATH}"], text=True
        )
    else:
        source = (REPO / SOURCE_PATH).read_text(encoding="utf-8")
    start = "void MainWindow::profile_stop(bool crash, bool block, bool manual) {"
    if source.count(start) != 1:
        raise AssertionError("profile_stop signature/extraction seam changed")
    # The function's closing brace is at column zero, unlike its nested scopes.
    match = re.search(re.escape(start) + r"\n.*?^\}", source, re.M | re.S)
    if match is None:
        raise AssertionError("profile_stop closing-brace extraction seam changed")
    return match.group(0), source.count("\n", 0, match.start()) + 1


class ProfileStopCompileTest(unittest.TestCase):
    def test_exact_function_compiles(self):
        function, line = read_function()
        stub = Path(__file__).with_name("profile-stop-compile") / "stubs.hpp"
        with tempfile.TemporaryDirectory(prefix="nekobox-profile-stop-") as directory:
            unit = Path(directory) / SOURCE_PATH
            unit.parent.mkdir(parents=True)
            # Keep diagnostics aligned with this source revision, including --ref.
            # A #line naming the real checkout could display its different text.
            unit.write_text(
                '#include "stubs.hpp"\n' + "\n" * (line - 2) + function + "\n",
                encoding="utf-8",
            )
            compiler = shlex.split(os.environ.get("CXX", "g++"))
            command = compiler + [
                "-std=c++20", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
                "-I", str(stub.parent), SOURCE_PATH,
            ]
            print(f"SOURCE {REF or 'working tree'}; exact function SHA256 "
                  f"{hashlib.sha256(function.encode()).hexdigest()}", flush=True)
            result = subprocess.run(command, cwd=directory, text=True,
                                    capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_stopping_snapshot_is_retained_and_captured_by_value(self):
        function, _ = read_function()
        snapshot = "const auto stoppingName = stopping->DisplayTypeAndName();"
        worker = "runOnNewThread([=, this, &blocker] {"
        log = 'MW_show_log(">>>>>>>> " + tr("Stopping profile %1").arg(stoppingName));'
        self.assertEqual(function.count(snapshot), 1)
        self.assertEqual(function.count("stoppingName"), 2)
        self.assertNotIn("running->DisplayTypeAndName()", function)
        self.assertEqual(function.count(worker), 1)
        self.assertEqual(function.count(log), 1)
        self.assertLess(function.index("auto stopping = runningCopy();"), function.index(snapshot))
        self.assertLess(function.index(snapshot), function.index(worker))
        self.assertLess(function.index(worker), function.index(log))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=REPO)
    parser.add_argument("--ref", help="Read source from this Git revision")
    args = parser.parse_args()
    REPO = args.repo.resolve()
    REF = args.ref
    unittest.main(argv=[__file__], verbosity=2)
