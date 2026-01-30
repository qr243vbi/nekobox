package main

import (
	"context"
	"fmt"
	"log"
	"nekobox_core/gen"
	"os"
	"os/exec"
	"syscall"
	"time"
	"kernel.org/pub/linux/libs/security/libcap/cap"
)


func (s *server) SetSystemDNS(ctx context.Context, in *gen.SetSystemDNSRequest) (*gen.EmptyResp, error) {
	out := new(gen.EmptyResp)
	return out, nil
}

func runAdmin(_port *int, _debug *bool) (int, error) {
	return 0, nil
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

func isElevated() (bool, error) {
	if os.Geteuid() == 0 {
		return true, nil
	}
	c := cap.GetProc()
	cap, err := c.GetFlag(cap.Effective, cap.NET_ADMIN)
	return cap, err
}

func restartAsAdmin(save bool) {
	elevated, err := isElevated()
	if elevated {
		if (save){
			caps:= cap.NewSet()
			err = caps.SetFlag(cap.Inheritable, true, cap.NET_ADMIN)
			err = caps.SetFlag(cap.Effective, true, cap.NET_ADMIN)
			err = caps.SetFlag(cap.Permitted, true, cap.NET_ADMIN)
			if (err != nil){
				panic(err)
			}
			err = caps.SetFile(os.Args[0])
			if (err != nil){
				panic(err)
			}
		}
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

	executablePath := os.Args[0]
	args = append(args, pkexecPath, "sh", "-c", "exec \"${0}\" \"${@}\"", "env", "NEKOBOX_APPIMAGE_CUSTOM_EXECUTABLE=nekobox_core", executablePath)

	for _, arg := range os.Args[1:] {
		if arg != "-admin" {
			args = append(args, arg)
		}
	}
	err = syscall.Exec(pkexecPath, args, os.Environ())
	if err != nil {
		// This part of the code will only be reached if syscall.Exec fails
		fmt.Println("Error executing 'pkexec':", err)
		os.Exit(1)
	}
}

func InstallerMode() {

}
