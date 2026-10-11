package main

import (
	"path/filepath"
	"syscall"
	"testing"
)

func TestNSISCommandPreservesExecutableAndDestination(t *testing.T) {
	for _, destination := range []string{`D:\NekoBox`, `D:\Portable Apps\NekoBox`, `C:\Program Files\NekoBox`, `D:\便携 应用\NekoBox`, `\\server\Shared Apps\NekoBox`, `D:\Portable Apps\NekoBox\..\NekoBox\`} {
		for _, chocolatey := range []bool{false, true} {
			const installer = `C:\Downloaded Updates\NekoBox installer.exe`
			command, err := newNSISCommand(installer, destination, chocolatey)
			if err != nil {
				t.Fatal(err)
			}
			if command.Path != installer {
				t.Fatalf("executable = %q, want %q", command.Path, installer)
			}
			flag := "0"
			if chocolatey {
				flag = "1"
			}
			want := syscall.EscapeArg(installer) + " /S /UNPACK=1 /CHOCOLATEY=" + flag + " /D=" + filepath.Clean(destination)
			if command.SysProcAttr == nil || command.SysProcAttr.CmdLine != want {
				t.Fatalf("command attributes = %#v, want command line %q", command.SysProcAttr, want)
			}
			if got := nsisDestination(command.SysProcAttr.CmdLine); got != filepath.Clean(destination) {
				t.Fatalf("NSIS destination = %q, want %q", got, filepath.Clean(destination))
			}
		}
	}
}

func TestNSISCommandRejectsInvalidPaths(t *testing.T) {
	for _, path := range []string{"", `relative\NekoBox`, `C:relative`, `\rooted`, `C:\"quoted"`, "C:\\Apps\x00\\NekoBox", "C:\\Apps\n\\NekoBox", "C:\\Apps\r\\NekoBox", "C:\\Apps\t\\NekoBox"} {
		if command, err := newNSISCommand(`C:\Downloads\installer.exe`, path, false); err == nil || command != nil {
			t.Errorf("invalid destination %q accepted: command %v, error %v", path, command, err)
		}
		if command, err := newNSISCommand(path, `D:\NekoBox`, false); err == nil || command != nil {
			t.Errorf("invalid installer %q accepted: command %v, error %v", path, command, err)
		}
	}
}
