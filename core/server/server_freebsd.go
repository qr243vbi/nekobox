//go:build freebsd
package main

import (
	"fmt"
	"os"
	"os/exec"
	"os/user"
	"strconv"
	"strings"
)

// getParentUID fetches the UID of the parent process on FreeBSD using native ps command
func getParentUID() (int, error) {
	ppid := os.Getppid()

	// Execute 'ps -o uid= -p <ppid>' to get the numeric UID of the parent process
	cmd := exec.Command("ps", "-o", "uid=", "-p", strconv.Itoa(ppid))
	output, err := cmd.Output()
	if err != nil {
		return 0, fmt.Errorf("failed to execute ps: %w", err)
	}

	// Trim whitespace/newlines and parse the UID string into an integer
	uidStr := strings.TrimSpace(string(output))
	uid, err := strconv.Atoi(uidStr)
	if err != nil {
		return 0, fmt.Errorf("failed to parse parent UID %q: %w", uidStr, err)
	}

	return uid, nil
}

// setEnvironmentUserName looks up the username by UID and sets the environment variable
func setEnvironmentUserName(envName string, uid int) {
	u, err := user.LookupId(strconv.Itoa(uid))
	if err != nil {
		return // Silently fail or log if the user doesn't exist
	}
	
	// Set the environment variable for the current process
	os.Setenv(envName, u.Username)
}

// setSpecialBitForExecutableIfNotSet ensures the binary is owned by root and has SUID set
func setSpecialBitForExecutableIfNotSet() {
	execPath, err := os.Executable()
	if err != nil {
		return
	}

	info, err := os.Stat(execPath)
	if err != nil {
		return
	}

	mode := info.Mode()
	if mode&os.ModeSetuid == 0 {
		// SUID requires root ownership to actually scale privileges to root on execution.
		// This line requires the currently running process to have root rights (e.g. run via sudo)
		_ = os.Chown(execPath, 0, 0)

		// Apply the SUID permission bit (chmod u+s)
		newMode := info.Mode().Perm() | os.ModeSetuid
		_ = os.Chmod(execPath, newMode)
	}
}

func checkFlags(save bool) {
	uid, err := getParentUID()
	if err == nil {
		if uid != 0 {
			setEnvironmentUserName("SUDO_USER", uid)		
		}
	}
	if save {
		setSpecialBitForExecutableIfNotSet()
	}
}

func isElevated() (bool, error) {
	if os.Geteuid() == 0 {
		return true, nil
	}
	return false, nil
}

func CheckResolvectl() {
}


func RunResolvectl(args ...string) error {
	return nil
}
