//go:build linux || freebsd

package main

import (
	"context"
	"fmt"
	"log"
	"nekobox_core/gen"
	"nekobox_core/internal"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"syscall"
	"time"
)

func (s *server) SetSystemDNS(ctx context.Context, in *gen.SetSystemDNSRequest) (*gen.EmptyResp, error) {
	out := new(gen.EmptyResp)
	return out, nil
}

func runAdmin() (int, error) {
	return 0, nil
}

func checkTaskScheduler(save bool) error {
	return nil
}

func doRun(_ string) error {
	return nil
}

func (s *server) IsPrivileged(ctx context.Context, in *gen.EmptyReq) (*gen.IsPrivilegedResponse, error) {
	out := new(gen.IsPrivilegedResponse)
	priv, err := isElevated()
	out.HasPrivilege = priv
	return out, err
}

func WaitForProcessExit(pid int) error {
	// Wait for the process to terminate
	for {
		// Send signal 0 to check if the process exists
		err := syscall.Kill(pid, syscall.Signal(0))
		if err != nil {
			if err == syscall.ESRCH {
				return nil
			}
			return fmt.Errorf("Error checking process status: %v", err)
		}
		time.Sleep(442 * time.Millisecond)
	}
}

func wsainit() {

}

/*
const polkitRuleDir = "/etc/polkit-1/rules.d/"
const polkitRuleFile = "10_nekobox_core.rules"
const polkitRuleContent = `
polkit.addRule(function(action, subject) {
    if ((action.id == "org.freedesktop.resolve1.set-domains" ||
         action.id == "org.freedesktop.resolve1.set-default-route" ||
         action.id == "org.freedesktop.resolve1.revert" ||
         action.id == "org.freedesktop.resolve1.set-dns-servers") &&
        subject.isInGroup("sing-box")) {
        return polkit.Result.YES;
    }
});`

func CreatePolkitRule() {
	filePath := polkitRuleDir + polkitRuleFile
	os.MkdirAll(polkitRuleDir, 0755)
	// Check if the directory exists, create if not
	if _, err := os.Stat(polkitRuleDir); os.IsNotExist(err) {
		log.Fatalf("Directory does not exist: %v", polkitRuleDir)
	}
	// Create the rule file
	file, err := os.Create(filePath)
	if err != nil {
		log.Fatalf("Error creating rule file: %v", err)
	}
	defer file.Close()
	// Write the rule content to the file
	_, err = file.WriteString(polkitRuleContent)
	if err != nil {
		log.Fatalf("Error writing to rule file: %v", err)
	}
	fmt.Printf("Polkit rule written to: %s\n", filePath)
}
*/

func restartAsAdmin(save bool) {
	elevated, err := isElevated()
	if elevated {
		checkFlags(save)
		return
	}
	var args []string
	pkexecPath, err := exec.LookPath("pkexec")
	if err != nil {
		// exec.ErrNotFound is returned if the executable cannot be found.
		if err == exec.ErrNotFound {
			log.Fatalf("pkexec executable not found in PATH: %v", err)
		} else {
			log.Fatalf("Error finding pkexec executable: %v", err)
		}
	}

	u, err := user.Current()
	if err != nil {
		log.Fatalf("Cannot get username: %v", err)
	}

	executablePath, err := filepath.Abs(os.Args[0])
	args = append(args, pkexecPath, "sh", "-c", "exec \"${0}\" \"${@}\"",
		"env")
	environ := os.Environ()
	args = append(args, environ...)
	args = append(args, "DISPLAY="+os.Getenv("DISPLAY"), "SUDO_USER="+u.Username,
		"NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE=nekobox_core", executablePath, "core",
		"-ruleset-cache-directory", internal.GetRulesetCachedir())

	args = append(args, os.Args[1:]...)

	err = syscall.Exec(pkexecPath, args, environ)
	if err != nil {
		// This part of the code will only be reached if syscall.Exec fails
		fmt.Println("Error executing 'pkexec':", err)
		os.Exit(1)
	}
}

func InstallerMode() {

}
