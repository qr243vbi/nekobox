#!/usr/bin/env python3
"""Bounded offline source-linked checks for empty subscription post-processing.

Run from the repository root, or use --repo/--ref for an immutable baseline.
The callback, FillProfileEnts, AsyncUpdate, and menu handler are compiled verbatim.
Qt scheduling, storage, validation, import, and GUI boundaries are synthetic;
the outer AsyncUpdate job runs on a real std::thread. No network or VPN is used.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT
REF = None
HERE = Path(__file__).resolve().parent / "subscription-empty"
CASES = (
    "empty", "all_missing", "repeated_missing", "single_valid", "single_invalid",
    "mixed", "all_invalid", "repeated_loaded_id", "empty_url_test", "mixed_url_test",
    "disabled", "limit_3000", "over_limit", "duplicate_stage", "repeat_update", "scheduling_failure",
)


def read(path):
    if REF:
        return subprocess.check_output(
            ["git", "-C", str(REPO), "show", f"{REF}:{path}"], text=True, timeout=10)
    return (REPO / path).read_text(encoding="utf-8")


def section(source, start, end):
    if source.count(start) != 1 or source.count(end) != 1:
        raise AssertionError(f"Production seam changed: {start!r}, {end!r}")
    return source[source.index(start):source.index(end, source.index(start))]


class EmptySubscriptionCallback(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="nekobox-subscription-empty-")
        cls.addClassCleanup(cls.temp.cleanup)
        temporary = Path(cls.temp.name)
        window = read("src/gharqad/ui/mainwindow.cpp")
        updater = read("src/gharqad/configs/sub/GroupUpdater.cpp")
        database = read("src/gharqad/dataStore/Database.cpp")
        extracted = "void MainWindow::installPostUpdateJob() {\n" + section(window,
            "  post_update_job = [this](std::shared_ptr<Configs::Group> group) {",
            "\n  mainwindow = this;") + "\n}\nnamespace Configs {\n" + section(database,
            "void ProfileManager::FillProfileEnts(", "int ProfileManager::GetProfileLatency(")
        extracted += "\n}\nnamespace Subscription {\n" + section(updater,
            "void GroupUpdater::AsyncUpdate(", "void GroupUpdater::Update(") + "\n}\n"
        extracted += section(window, "bool mw_sub_updating = false;",
                             "void MainWindow::on_menu_remove_unavailable_triggered()")
        (temporary / "production.inc").write_text(extracted, encoding="utf-8")
        cls.binary = temporary / "regression"
        command = shlex.split(os.environ.get("CXX", "g++")) + [
            "-std=c++20", "-O0", "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-fsanitize=undefined", "-fno-sanitize-recover=all",
            "-I", str(temporary), "-I", str(HERE), str(HERE / "regression.cpp"),
            "-o", str(cls.binary),
        ]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        print(f"SOURCE {REF or 'working tree'}; excerpt SHA256 "
              f"{hashlib.sha256(extracted.encode()).hexdigest()}", flush=True)

    def run_case(self, case):
        for schedule in ("deferred_fifo", "deferred_reverse", "finish_before_wait"):
            with self.subTest(schedule=schedule):
                result = subprocess.run([str(self.binary), case, schedule],
                                        capture_output=True, text=True, timeout=10)
                print(result.stdout, end="", flush=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


for case in CASES:
    setattr(EmptySubscriptionCallback, "test_" + case,
            lambda self, name=case: self.run_case(name))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=ROOT)
    parser.add_argument("--ref", help="Read production files from this immutable Git ref")
    args = parser.parse_args()
    REPO = args.repo.resolve()
    REF = args.ref
    unittest.main(argv=[__file__], verbosity=2)
