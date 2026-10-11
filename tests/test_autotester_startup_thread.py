"""Run production auto-tester startup with real Qt timers and a worker caller.

RPC, traffic loops and health/test work are inert boundaries. No application
settings, network, TUN interface or installed NekoBox process is touched.
AUTOTESTER_REF selects old production source for the regression check.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_link_tls import function

ROOT = Path(__file__).resolve().parents[1]
HERE = ROOT / "tests/autotester-startup-thread"


def read_source(path):
    ref = os.environ.get("AUTOTESTER_REF")
    if ref:
        return subprocess.check_output(["git", "-C", str(ROOT), "show", f"{ref}:{path}"], text=True)
    return (ROOT / path).read_text()


class AutoTesterStartupThreadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="nekobox-autotester-startup-")
        cls.addClassCleanup(cls.temp.cleanup)
        build = Path(cls.temp.name)
        header = read_source("src/nekobox/stats/autotester/ProxyAutoTester.hpp")
        (build / "ProxyAutoTester.hpp").write_text(header)
        source = read_source("src/gharqad/stats/autotester/ProxyAutoTester.cpp")
        signatures = (
            "ProxyAutoTester::ProxyAutoTester(QObject *parent)",
            "ProxyAutoTester::~ProxyAutoTester()",
            "void ProxyAutoTester::Start()",
            "void ProxyAutoTester::Stop()",
            "void ProxyAutoTester::onTestTimerTimeout()",
            "void ProxyAutoTester::onHealthCheckTimerTimeout()",
            "bool ProxyAutoTester::isEnabled() const",
            "int ProxyAutoTester::getTestInterval() const",
        )
        bodies = "\n".join(function(source, signature) for signature in signatures)
        setup = function(read_source("src/gharqad/ui/mainwindow_rpc.cpp"), "void MainWindow::setup_rpc()")
        fixture = (HERE / "checks.cpp.in").read_text()
        fixture = fixture.replace("@BODIES@", bodies).replace("@SETUP_RPC@", setup)
        (build / "checks.cpp").write_text(fixture)
        libexec = Path(subprocess.check_output(
            ["pkg-config", "--variable=libexecdir", "Qt6Core"], text=True).strip())
        subprocess.run([str(libexec / "moc"), str(build / "ProxyAutoTester.hpp"),
                        "-o", str(build / "moc_autotester.cpp")], check=True)
        flags = shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "Qt6Core"], text=True))
        cls.exe = build / "checks"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-fPIC", "-Wall", "-Wextra",
                        *shlex.split(os.environ.get("EXTRA_CXXFLAGS", "")),
                        str(build / "checks.cpp"), str(build / "moc_autotester.cpp"),
                        "-o", str(cls.exe), *flags], check=True, timeout=90)

    def check_case(self, case):
        result = subprocess.run([str(self.exe), case], capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_core_worker_starts_tun_health_timers(self):
        self.check_case("worker-tun")

    def test_core_worker_starts_proxy_health_timers(self):
        self.check_case("worker-proxy")

    def test_gui_startup_keeps_timers_working(self):
        self.check_case("gui")

    def test_disabled_startup(self):
        self.check_case("disabled")

    def test_disable_before_queued_start(self):
        self.check_case("disable-pending")

    def test_tester_destroyed_before_queued_start(self):
        self.check_case("destroy-pending")

    def test_stop_cancels_periodic_callbacks(self):
        self.check_case("stop")


if __name__ == "__main__":
    unittest.main()
