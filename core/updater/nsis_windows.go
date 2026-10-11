package main

import (
	"fmt"
	"os/exec"
	"path/filepath"
	"syscall"
)

func newNSISCommand(installerPath, installPath string, chocolatey bool) (*exec.Cmd, error) {
	for _, path := range []string{installerPath, installPath} {
		if err := validateNSISPath(path); err != nil {
			return nil, err
		}
		if !filepath.IsAbs(path) {
			return nil, fmt.Errorf("installer and installation paths must be absolute")
		}
	}
	installerPath = filepath.Clean(installerPath)
	arguments, err := nsisInstallArguments(filepath.Clean(installPath), chocolatey)
	if err != nil {
		return nil, err
	}
	command := exec.Command(installerPath)
	// Go's normal argument escaping quotes a space-bearing /D argument, which
	// NSIS ignores. Preserve standard executable quoting, but pass NSIS its
	// documented raw final directory switch directly, without a command shell.
	command.SysProcAttr = &syscall.SysProcAttr{
		CmdLine: syscall.EscapeArg(installerPath) + " " + arguments,
	}
	return command, nil
}
