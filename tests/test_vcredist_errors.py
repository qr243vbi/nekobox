#!/usr/bin/env python3
"""Offline, source-linked regressions for Windows prerequisite error reporting.

Run: python3 tests/test_vcredist_errors.py [--repo PATH] [--ref COMMIT]
GO may point to a Go compiler. No Go dependencies are downloaded by this test.
The three production Go functions are compiled verbatim. The filesystem lookup,
HTTP download, external command, architecture and process-killing boundaries are
inert mocks. The NSIS test interprets only the prerequisite helper block, rejecting
unknown instructions; it does not replace a real NSIS build or Windows UI test.
No installer, registry or system settings are touched.
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
GO_SOURCE = "core/server/server_windows.go"
NSIS_SOURCE = "script/windows_installer.nsi"


def read_source(path):
    if REF:
        return subprocess.check_output(
            ["git", "-C", str(REPO), "show", f"{REF}:{path}"], text=True
        )
    return (REPO / path).read_text(encoding="utf-8")


def function(source, name):
    anchor = f"func {name}("
    if source.count(anchor) != 1:
        raise ValueError(f"Expected exactly one {anchor}")
    start = source.index(anchor)
    return source[start:source.index("\n}", start) + 2]


GO_FIXTURE = r'''
package main
import (
    "flag"
    "fmt"
    "os"
    "path/filepath"
    "strconv"
    exec "fixture/mockexec"
)
var runtime = struct{ GOARCH string }{os.Getenv("VCREDIST_ARCH")}
func getDownloadDir() string {
    if os.Getenv("VCREDIST_SCENARIO") == "missing_downloads" { return "" }
    return "mock-downloads"
}
func fileExists(path string) bool {
    return os.Getenv("VCREDIST_SCENARIO") == "cached_success"
}
func DownloadWithProgress(url, path string) error {
    fmt.Printf("MOCK_DOWNLOAD %s %s\n", url, path)
    if os.Getenv("VCREDIST_SCENARIO") == "download_error" {
        return fmt.Errorf("mock network unavailable")
    }
    return nil
}
func KillProcesses(path string, tags map[uint32]bool) {
    fmt.Printf("MOCK_KILL %s ignored=%t\n", path, tags[42])
}
func main() {
    os.Args = []string{"nekobox_core", "-installer-mode", "-kill-processes", "mock-install", "-ignore-pid", "42"}
    if os.Getenv("VCREDIST_SCENARIO") != "no_redist" {
        os.Args = append(os.Args, "-vcredist-install")
    }
    InstallerMode()
}
'''

EXEC_FIXTURE = r'''
package mockexec
import (
    "fmt"
    "os"
    "strconv"
    "strings"
)
type Cmd struct{ name string }
type ExitError struct{ code int }
func (e *ExitError) Error() string { return fmt.Sprintf("exit status %d", e.code) }
func (e *ExitError) ExitCode() int { return e.code }
func Command(name string, args ...string) *Cmd { return &Cmd{name: name} }
func (c *Cmd) Run() error {
    fmt.Printf("MOCK_EXEC %s\n", c.name)
    scenario := os.Getenv("VCREDIST_SCENARIO")
    if scenario == "launch_error" { return fmt.Errorf("mock executable cannot start") }
    if strings.HasPrefix(scenario, "exit_") {
        code, err := strconv.Atoi(strings.TrimPrefix(scenario, "exit_"))
        if err != nil { panic(err) }
        return &ExitError{code: code}
    }
    return nil
}
'''


class VCRedistCoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.go = shlex.split(os.environ.get("GO", "go"))
        try:
            version = subprocess.run(cls.go + ["version"], text=True,
                                     capture_output=True, timeout=10)
        except OSError as error:
            raise unittest.SkipTest(f"Go compiler unavailable: {error}")
        if version.returncode != 0 or not version.stdout.startswith("go version go"):
            raise unittest.SkipTest("GO must point to a Go compiler")
        cls.temp = tempfile.TemporaryDirectory(prefix="nekobox-vcredist-")
        cls.addClassCleanup(cls.temp.cleanup)
        root = Path(cls.temp.name)
        cls.root = root
        source = read_source(GO_SOURCE)
        functions = "\n\n".join(function(source, name) for name in (
            "InstallVcRedist", "mustAtoi", "InstallerMode"
        ))
        print(f"SOURCE {REF or 'working tree'} {GO_SOURCE} SHA256 "
              f"{hashlib.sha256(source.encode()).hexdigest()}", flush=True)
        (root / "main.go").write_text(GO_FIXTURE + "\n" + functions)
        (root / "go.mod").write_text("module fixture\n\ngo 1.20\n")
        (root / "mockexec").mkdir()
        (root / "mockexec/exec.go").write_text(EXEC_FIXTURE)
        cls.binary = root / ("fixture.exe" if os.name == "nt" else "fixture")
        env = os.environ.copy()
        for key in ("GOOS", "GOARCH", "GOROOT", "GOFLAGS"):
            env.pop(key, None)
        env.setdefault("GOCACHE", str(root / "go-cache"))
        env.update(GOTOOLCHAIN="local", GOPROXY="off", GOSUMDB="off", GOWORK="off", CGO_ENABLED="0")
        compiled = subprocess.run(cls.go + ["build", "-buildvcs=false", "-o", str(cls.binary), "."],
                                  cwd=root, env=env, text=True, capture_output=True, timeout=120)
        if compiled.returncode:
            raise AssertionError(compiled.stdout + compiled.stderr)

    def test_functions_compile_for_supported_windows_architectures(self):
        # Compile only; never execute these Windows binaries. Use the real
        # standard-library exec/runtime types instead of the behavioral mocks.
        root = self.root / "windows-compile"
        root.mkdir()
        source = (self.root / "main.go").read_text()
        source = source.replace('exec "fixture/mockexec"', '"os/exec"\n    "runtime"')
        source = source.replace('var runtime = struct{ GOARCH string }{os.Getenv("VCREDIST_ARCH")}\n', '')
        (root / "main.go").write_text(source)
        (root / "go.mod").write_text("module windowsfixture\n\ngo 1.20\n")
        env = os.environ.copy()
        for key in ("GOROOT", "GOFLAGS"):
            env.pop(key, None)
        env.setdefault("GOCACHE", str(self.root / "go-cache"))
        env.update(GOTOOLCHAIN="local", GOPROXY="off", GOSUMDB="off",
                   GOWORK="off", CGO_ENABLED="0", GOOS="windows")
        for arch in ("amd64", "arm64", "386"):
            with self.subTest(arch=arch):
                env["GOARCH"] = arch
                result = subprocess.run(
                    self.go + ["build", "-buildvcs=false", "-o", f"fixture-{arch}.exe", "."],
                    cwd=root, env=env, text=True, capture_output=True, timeout=120,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def run_mode(self, scenario="success", arch="amd64"):
        env = os.environ.copy()
        env.update(VCREDIST_SCENARIO=scenario, VCREDIST_ARCH=arch)
        return subprocess.run([str(self.binary)], env=env, text=True,
                              capture_output=True, timeout=10)

    def assert_failure(self, scenario, diagnostic, arch="amd64"):
        result = self.run_mode(scenario, arch)
        output = result.stdout + result.stderr
        self.assertNotEqual(result.returncode, 0, output)
        self.assertIn(diagnostic, output)
        self.assertNotIn("MOCK_KILL", output)
        return output

    def test_missing_download_directory_reports_failure(self):
        output = self.assert_failure("missing_downloads", "Downloads")
        self.assertNotIn("MOCK_EXEC", output)

    def test_download_failure_stops_before_execution(self):
        output = self.assert_failure("download_error", "mock network unavailable")
        self.assertNotIn("MOCK_EXEC", output)

    def test_launch_failure_reports_failure(self):
        self.assert_failure("launch_error", "mock executable cannot start")

    def test_cancelled_installer_reports_failure(self):
        self.assert_failure("exit_1602", "1602")

    def test_failed_installer_reports_failure(self):
        self.assert_failure("exit_1603", "1603")

    def test_unsupported_architecture_reports_failure(self):
        output = self.assert_failure("success", "unsupported", arch="unsupported")
        self.assertNotIn("MOCK_DOWNLOAD", output)
        self.assertNotIn("MOCK_EXEC", output)

    def test_success_preserves_each_supported_package(self):
        for arch, suffix in (("amd64", "x64"), ("arm64", "arm64"), ("386", "x86")):
            with self.subTest(arch=arch):
                result = self.run_mode(arch=arch)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(f"https://aka.ms/vc14/vc_redist.{suffix}.exe", result.stdout)
                self.assertIn(f"vc14_redist.{suffix}.exe", result.stdout)
                self.assertIn("MOCK_KILL mock-install ignored=true", result.stdout)

    def test_cached_installer_does_not_download_again(self):
        result = self.run_mode("cached_success")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("MOCK_DOWNLOAD", result.stdout)
        self.assertIn("MOCK_EXEC", result.stdout)

    def test_success_reboot_codes_are_not_failures(self):
        for code in (1641, 3010):
            with self.subTest(code=code):
                result = self.run_mode(f"exit_{code}")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("MOCK_KILL", result.stdout)

    def test_non_prerequisite_mode_is_unchanged(self):
        result = self.run_mode("no_redist")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("MOCK_DOWNLOAD", result.stdout)
        self.assertNotIn("MOCK_EXEC", result.stdout)
        self.assertIn("MOCK_KILL mock-install ignored=true", result.stdout)


def run_nsis_block(source, needed, exit_status):
    """Execute the actual narrow NSIS block against inert command/UI boundaries."""
    start = source.index('  ${If} "$VCRedistNeeded" != "1"')
    end = source.index('  !insertmacro MoveFile ', start)
    block = re.sub(r"\\\r?\n\s*", "", source[start:end])
    variables = {"$VCRedistNeeded": needed}
    stack, conditions, messages, commands = [], [], [], []
    cleanup = []
    errorlevel = 0
    aborted = False

    def expand(value):
        return re.sub(r"\$(?:[A-Za-z][A-Za-z0-9]*|[0-9])", lambda m: variables.get(m[0], m[0]), value)

    for raw in block.splitlines():
        line = raw.strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("${If} "):
            condition = re.fullmatch(r'\$\{If\}\s+"?([^"\s]+)"?\s+(==|!=)\s+"?([^"\s]+)"?', line)
            if condition is None:
                raise AssertionError("Unsupported NSIS condition: " + line)
            left, operator, right = condition.groups()
            equal = expand(left) == expand(right)
            conditions.append(equal if operator == "==" else not equal)
            continue
        if line == "${Else}":
            conditions[-1] = not conditions[-1]
            continue
        if line == "${EndIf}":
            conditions.pop()
            continue
        if not all(conditions):
            continue
        if line.startswith("nsExec::ExecToLog "):
            commands.append(line)
            stack.append(exit_status)
        elif line.startswith("Pop "):
            variables[line.split()[1]] = stack.pop()
        elif line.startswith("MessageBox "):
            messages.append(expand(line))
        elif line.startswith("DetailPrint "):
            pass
        elif line.startswith("SetErrorLevel "):
            errorlevel = int(line.split()[1])
        elif line.startswith(("SetOutPath ", "Delete ", "RMDir ")):
            cleanup.append(line)
        elif line == "Abort":
            cleanup.append(line)
            aborted = True
            break
        else:
            raise AssertionError("Unsupported NSIS instruction: " + line)
    return dict(aborted=aborted, errorlevel=errorlevel, messages=messages, stack=stack, commands=commands, cleanup=cleanup)


class VCRedistInstallerTests(unittest.TestCase):
    def test_failed_prerequisite_warns_and_stops_before_copy(self):
        for status in ("1", "error", "timeout"):
            with self.subTest(status=status):
                result = run_nsis_block(read_source(NSIS_SOURCE), "1", status)
                self.assertTrue(result["aborted"], result)
                self.assertNotEqual(result["errorlevel"], 0, result)
                self.assertEqual(result["stack"], [], result)
                message = " ".join(result["messages"])
                self.assertIn("Visual C++", message)
                self.assertIn("https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist", message)
                self.assertIn(status, message)
                self.assertIn("/SD IDOK", message)

    def test_failed_prerequisite_cleans_only_its_helper_before_abort(self):
        for status in ("1", "error", "timeout"):
            with self.subTest(status=status):
                result = run_nsis_block(read_source(NSIS_SOURCE), "1", status)
                self.assertEqual(result["cleanup"], [
                    'SetOutPath "$INSTDIR"',
                    r'Delete "$INSTDIR\$RandomGUID\nekobox_core.exe"',
                    r'RMDir "$INSTDIR\$RandomGUID"',
                    'Abort',
                ])

    def test_successful_prerequisite_continues_without_warning(self):
        result = run_nsis_block(read_source(NSIS_SOURCE), "1", "0")
        self.assertFalse(result["aborted"], result)
        self.assertEqual(result["errorlevel"], 0, result)
        self.assertEqual(result["messages"], [], result)
        self.assertEqual(result["cleanup"], [], result)
        self.assertEqual(result["stack"], [], result)
        self.assertIn("-vcredist-install", result["commands"][0])

    def test_skipped_prerequisite_retains_kill_only_command(self):
        for needed in ("0", ""):
            with self.subTest(needed=needed):
                result = run_nsis_block(read_source(NSIS_SOURCE), needed, "0")
                self.assertFalse(result["aborted"], result)
                self.assertEqual(result["messages"], [], result)
                self.assertEqual(result["cleanup"], [], result)
                self.assertEqual(result["stack"], [], result)
                self.assertNotIn("-vcredist-install", result["commands"][0])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=REPO)
    parser.add_argument("--ref", help="Read production source from this Git revision")
    args = parser.parse_args()
    REPO, REF = args.repo.resolve(), args.ref
    unittest.main(argv=[__file__], verbosity=2)
