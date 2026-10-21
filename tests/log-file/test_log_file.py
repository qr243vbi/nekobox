#!/usr/bin/env python3
"""The optional file log (NEKOBOX_LOG_FILE) of src/nekobox/global/LogFile.hpp.

Compiles the production header and the production sanitizeLog() from
Utils.cpp against Qt6 Core, runs each case in a fresh process (the path is
resolved once per process) and checks the file it leaves behind. No
application, core, profile, setting or network is used.
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE.parent))
from test_link_tls import function  # noqa: E402

STAMP = r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} "


def compile_command(cpp, exe, includes):
    cxx = os.environ.get("CXX")
    qt_dir = os.environ.get("QT_DIR")
    if (cxx and Path(cxx).name.lower().startswith("cl")) or (not cxx and qt_dir and not shutil.which("pkg-config")):
        # MSVC, from a developer prompt; QT_DIR is the Qt kit root
        qt = Path(qt_dir)
        return [cxx or "cl", "/nologo", "/std:c++20", "/EHsc", "/permissive-", "/Zc:__cplusplus", "/utf-8",
                "/W3", "/we4018", *[f"/I{i}" for i in includes], f"/I{qt / 'include'}",
                f"/I{qt / 'include' / 'QtCore'}", str(cpp), f"/Fe{exe}", f"/Fo{Path(cpp).parent}\\",
                "/link", f"/LIBPATH:{qt / 'lib'}", "Qt6Core.lib"]
    probe = subprocess.run(["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True, capture_output=True)
    if probe.returncode:
        raise SystemExit("BLOCKED: Qt6 Core development headers/pkg-config metadata unavailable.")
    return (shlex.split(cxx or "c++") + ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-O0", "-fPIC"]
            + [f"-I{i}" for i in includes] + [str(cpp), "-o", str(exe)] + shlex.split(probe.stdout))


class LogFileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="nekobox-log-file-")
        cls.addClassCleanup(cls.temp.cleanup)
        work = Path(cls.temp.name)
        # LogFile.hpp needs only sanitizeLog() from Utils.hpp: the production
        # body, behind a header that declares nothing else
        stub = work / "include" / "nekobox" / "dataStore"
        stub.mkdir(parents=True)
        utils = (REPO / "src/gharqad/dataStore/Utils.cpp").read_text(encoding="utf-8")
        (stub / "Utils.hpp").write_text(
            "#pragma once\n#include <QRegularExpression>\n#include <QString>\n#include <QStringView>\n"
            "inline " + function(utils, "QString sanitizeLog(const QStringView &raw)") + "\n",
            encoding="utf-8")
        cpp = work / "checks.cpp"
        cpp.write_text((HERE / "checks.cpp").read_text(encoding="utf-8"), encoding="utf-8")
        cls.exe = work / ("checks.exe" if os.name == "nt" else "checks")
        # mutation_checks.py puts a changed copy of LogFile.hpp in front
        mutant = os.environ.get("LOG_FILE_TEST_INCLUDE")
        includes = ([Path(mutant)] if mutant else []) + [work / "include", REPO / "src"]
        subprocess.run(compile_command(cpp, cls.exe, includes),
                       check=True, timeout=240, cwd=work)
        cls.work = work

    def run_case(self, mode, log_path, cwd=None, *args):
        env = dict(os.environ)
        env.pop("NEKOBOX_LOG_FILE", None)
        if log_path is not None:
            env["NEKOBOX_LOG_FILE"] = str(log_path)
        result = subprocess.run([str(self.exe), mode, *map(str, args)], env=env, cwd=cwd or self.work,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def lines(self, path):
        return path.read_text(encoding="utf-8").splitlines()

    def test_disabled_without_variable(self):
        case = self.work / "disabled"
        case.mkdir()
        out = self.run_case("disabled", None, cwd=case)
        self.assertIn("enabled=0", out)
        self.assertEqual(list(case.iterdir()), [])

    def test_lines_are_stamped_cleaned_and_split(self):
        case = self.work / "lines"
        elsewhere = case / "elsewhere"
        elsewhere.mkdir(parents=True)
        out = self.run_case("lines", "relative.log", case, elsewhere)
        self.assertEqual(list(elsewhere.iterdir()), [])
        self.assertIn("enabled=1", out)
        lines = self.lines(case / "relative.log")  # relative to the start directory
        self.assertEqual(len(lines), 4, lines)
        for line in lines:
            self.assertRegex(line, "^" + STAMP)
        self.assertEqual([re.sub("^" + STAMP, "", line) for line in lines],
                         ["first line", "WARN coloured", "padded", "second call"])
        self.assertFalse(any("\x1b" in line for line in lines))

    def test_appends_to_an_existing_file(self):
        path = self.work / "existing.log"
        path.write_text("kept\n", encoding="utf-8")
        self.run_case("lines", path, None, self.work)
        lines = self.lines(path)
        self.assertEqual(lines[0], "kept")
        self.assertEqual(len(lines), 5)

    def test_qt_messages_are_mirrored_and_passed_on(self):
        path = self.work / "qt.log"
        out = self.run_case("qt", path)
        self.assertIn("previous=2", out)  # the earlier handler still gets them
        tails = [re.sub("^" + STAMP, "", line) for line in self.lines(path)]
        self.assertEqual(tails, ["[Qt warning] warned 1", "[Qt info] informed"])

    def test_concurrent_writers_keep_whole_lines(self):
        path = self.work / "threads.log"
        self.run_case("threads", path)
        lines = self.lines(path)
        self.assertEqual(len(lines), 8 * 200)
        pattern = re.compile("^" + STAMP + r"thread \d line \d+ x{40}$")
        self.assertTrue(all(pattern.match(line) for line in lines))

    def test_unwritable_path_is_harmless(self):
        target = self.work / "a-directory"
        target.mkdir()
        out = self.run_case("unwritable", target)
        self.assertIn("enabled=1", out)
        self.assertTrue(target.is_dir())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_known_args()
    unittest.main(verbosity=2)
