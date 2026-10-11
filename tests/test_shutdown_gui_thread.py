"""Exercise the production exit handler and overlay with an offscreen Qt event loop.

Run with unittest discovery, or set SHUTDOWN_REF to a git revision for a baseline.
Application cleanup/RPC is replaced at its entry boundary; no settings, core,
system proxy or network are touched. The real overlay and exit handler run.
"""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
HERE = ROOT / "tests/shutdown-gui-thread"


def read_source(path):
    ref = os.environ.get("SHUTDOWN_REF")
    if ref:
        return subprocess.check_output(["git", "-C", str(ROOT), "show", f"{ref}:{path}"], text=True)
    return (ROOT / path).read_text()


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class ShutdownGuiThreadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="nekobox-shutdown-")
        cls.addClassCleanup(cls.temp.cleanup)
        build = Path(cls.temp.name)
        overlay_header = read_source("src/nekobox/ui/setting/shutdownoverlay.hpp")
        (build / "shutdownoverlay.hpp").write_text(overlay_header)
        overlay = read_source("src/gharqad/ui/setting/shutdownoverlay.cpp")
        overlay = "\n".join(line for line in overlay.splitlines()
                            if not line.startswith("#include <nekobox/"))
        header = read_source("src/nekobox/ui/mainwindow.h")
        fields = "bool force_hide_tray = false;"
        if "bool exit_in_progress = false;" in header:
            fields += "\nbool exit_in_progress = false;"
        handler = function(read_source("src/gharqad/ui/mainwindow.cpp"),
                           "void MainWindow::on_menu_exit_triggered(bool faster)")
        fixture = (HERE / "checks.cpp.in").read_text()
        fixture = fixture.replace("@FIELDS@", fields).replace("@HANDLER@", handler)
        fixture = fixture.replace("@OVERLAY@", overlay)
        fixture = fixture.replace("@SIGNAL_HANDLER@", function(
            read_source("src/gharqad/main.cpp"), "void signal_handler(int signum)"))
        (build / "checks.cpp").write_text(fixture)
        libexec = subprocess.check_output(
            ["pkg-config", "--variable=libexecdir", "Qt6Core"], text=True).strip()
        subprocess.run([str(Path(libexec) / "moc"), str(build / "shutdownoverlay.hpp"),
                        "-o", str(build / "moc_overlay.cpp")], check=True)
        flags = shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "Qt6Widgets"], text=True))
        cls.exe = build / "checks"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fPIC", "-Wall", "-Wextra",
                        *shlex.split(os.environ.get("EXTRA_CXXFLAGS", "")),
                        str(build / "checks.cpp"), str(build / "moc_overlay.cpp"),
                        "-o", str(cls.exe), *flags], check=True, timeout=90)

    def check_case(self, case):
        result = subprocess.run([str(self.exe), case], capture_output=True, text=True,
                                timeout=8, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_normal_exit(self):
        self.check_case("normal")

    def test_fast_exit(self):
        self.check_case("fast")

    def test_worker_requested_exit(self):
        self.check_case("worker")

    def test_worker_requested_fast_exit(self):
        self.check_case("worker-fast")

    def test_repeated_exit_during_cleanup(self):
        self.check_case("reentrant")

    def test_declined_exit_restores_tray(self):
        self.check_case("declined")

    def test_overlay_dismissal_keeps_cleanup_alive(self):
        self.check_case("dismissed")

    def test_window_destroyed_before_cleanup(self):
        self.check_case("destroyed")

    def test_signal_requested_exit(self):
        self.check_case("signal")

    def test_signal_requested_exit_from_worker(self):
        self.check_case("worker-signal")

    def test_signal_during_cleanup(self):
        self.check_case("reentrant-signal")

    def test_fast_cleanup_finishes_before_caller_returns(self):
        self.check_case("fast-completed")

    def test_session_end_cleanup_finishes_before_caller_returns(self):
        self.check_case("signal-completed")

    def test_normal_exit_followed_immediately_by_session_end(self):
        self.check_case("pending-signal")


if __name__ == "__main__":
    unittest.main()
