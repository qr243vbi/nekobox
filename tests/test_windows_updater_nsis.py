"""Offline command and error-flow tests for the Windows NSIS updater.

Set GO_TEST_BINARY to a trusted Go compiler if `go` is not on PATH. The fixture
compiles the production main and LaunchInstaller functions verbatim, replacing
OS/process/flag/time boundaries with in-process fakes. It never starts an
installer, package manager, PowerShell, or NekoBox. Windows constructor tests
live in core/updater/nsis_windows_test.go and require Windows to execute.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
package main
import ("errors"; "fmt"; "log"; "path/filepath"; "testing")

type fakeCommand struct{}
func (*fakeCommand) Start() error { restarts++; return nil }
var exec = struct{ Command func(string, ...string) *fakeCommand }{
    Command: func(string, ...string) *fakeCommand { return &fakeCommand{} },
}
var time = struct{ Sleep func(int); Second int }{Sleep: func(int){}, Second: 1}
type fakeFlags struct{}
func (fakeFlags) String(_, value, _ string) *string { return &value }
func (fakeFlags) Bool(_ string, value bool, _ string) *bool { return &value }
func (fakeFlags) Parse() {}
func (fakeFlags) Args() []string { return []string{`C:\Downloads\setup.exe`, `D:\Portable Apps\NekoBox`} }
var flag fakeFlags
var os = struct{
    Executable func() (string,error)
    Args []string
    Chdir func(string) error
    Exit func(int)
}{
    Executable: func()(string,error){ return `C:\temp\updater.exe`,nil },
    Args: []string{`C:\temp\updater.exe`},
    Chdir: func(string)error{ changes++; return nil },
    Exit: func(code int){ panic(exitCode(code)) },
}
type exitCode int
var buildError, runError error
var restarts, changes, starts, dialogs, chocolateyStarts int
func newNSISCommand(string,string,bool)(*fakeCommand,error){
    if buildError != nil { return nil, buildError }; return &fakeCommand{},nil
}
func LaunchCmd(*fakeCommand)error{ starts++; return runError }
func Launch(string,...string)error{ panic("unexpected package-manager process") }
func run_chocolatey(string,string,string){ chocolateyStarts++ }
func MessageBoxPlain(string,string)int{ dialogs++; return 0 }
func reset(){ buildError=nil; runError=nil; restarts=0; changes=0; starts=0; dialogs=0; chocolateyStarts=0 }
func runMain() (code int){
    defer func(){ if p:=recover(); p!=nil { var ok bool; code,ok=asExit(p); if !ok {panic(p)} } }()
    main(); return 0
}
func asExit(p any)(int,bool){ e,ok:=p.(exitCode); return int(e),ok }
func TestRejectedCommandStopsBeforeProcesses(t *testing.T){
    reset(); buildError=errors.New("invalid path")
    if code:=runMain(); code!=1 { t.Fatalf("exit=%d, want 1",code) }
    if starts!=0 || restarts!=0 || changes!=0 || dialogs!=1 {t.Fatalf("starts=%d restarts=%d changes=%d dialogs=%d",starts,restarts,changes,dialogs)}
}
func TestFailedInstallerDoesNotRestart(t *testing.T){
    reset(); runError=errors.New("installer exit status 2")
    if code:=runMain(); code!=1 {t.Fatalf("exit=%d, want 1",code)}
    if starts!=1 || restarts!=0 || changes!=0 || dialogs!=1 {t.Fatalf("starts=%d restarts=%d changes=%d dialogs=%d",starts,restarts,changes,dialogs)}
}
func TestSuccessfulInstallerRestarts(t *testing.T){
    reset()
    if code:=runMain(); code!=0 {t.Fatalf("exit=%d, want 0",code)}
    if starts!=1 || restarts!=1 || changes!=1 || dialogs!=0 {t.Fatalf("starts=%d restarts=%d changes=%d dialogs=%d",starts,restarts,changes,dialogs)}
}
func TestRejectedCommandDoesNotStartChocolatey(t *testing.T){
    reset(); buildError=errors.New("invalid path")
    err:=LaunchInstaller(`C:\Downloads\setup.exe`, `D:\NekoBox`, "1.2.3", `C:\Packages`,false,false,"nekobox")
    if !errors.Is(err,buildError) || starts!=0 || chocolateyStarts!=0 {t.Fatalf("err=%v starts=%d chocolatey=%d",err,starts,chocolateyStarts)}
}
'''


class WindowsUpdaterNSISTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.go = os.environ.get("GO_TEST_BINARY") or shutil.which("go")
        if not cls.go:
            raise unittest.SkipTest("Go compiler unavailable; set GO_TEST_BINARY")
        result = subprocess.run([cls.go, "version"], capture_output=True, text=True)
        if result.returncode or not result.stdout.startswith("go version go"):
            raise unittest.SkipTest("Selected executable is not a Go compiler")

    def test_offline_arguments_and_production_error_flow(self):
        source = (ROOT / "core/updater/main_windows.go").read_text()
        start = source.index("func main() {")
        end = source.index("\nfunc run_chocolatey", start)
        functions = source[start:end]
        with tempfile.TemporaryDirectory(prefix="nekobox-nsis-test-") as directory:
            path = Path(directory)
            (path / "go.mod").write_text("module nsisfixture\n\ngo 1.23\n")
            (path / "flow_test.go").write_text(FIXTURE + "\n" + functions)
            for name in ("nsis_args.go", "nsis_args_test.go"):
                shutil.copyfile(ROOT / "core/updater" / name, path / name)
            env = os.environ.copy()
            env.update({"GOTOOLCHAIN": "local", "GOPROXY": "off", "GOSUMDB": "off", "GOWORK": "off", "GOOS": "", "GOARCH": ""})
            env.setdefault("GOCACHE", str(path / "cache"))
            result = subprocess.run([self.go, "test", "-buildvcs=false", "-v", "."], cwd=path, env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
