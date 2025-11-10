package main

import (
	"Core/gen"
	"os"
	"fmt"
	"time"
	"syscall"
	"log"
	"os/exec"
)

func (s *server) SetSystemDNS(in *gen.SetSystemDNSRequest, out *gen.EmptyResp) error {
	return nil
}

func runAdmin(_port * int, _debug * bool) (int, error) {
	return 0, nil
}

func (s *server) IsPrivileged(in *gen.EmptyReq, out *gen.IsPrivilegedResponse) error {
	out.HasPrivilege = To(os.Geteuid() == 0)
	return nil
}

func WaitForProcessExit (pid int) error{
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


func restartAsAdmin(){
	if (os.Geteuid() == 0){
		return
	}
	var args [] string
	pkexecPath, err := exec.LookPath("pkexec")
	if err != nil {
		// exec.ErrNotFound is returned if the executable cannot be found.
		if err == exec.ErrNotFound {
			log.Fatalf("pkexec executable not found in PATH: %v", err)
		} else {
			log.Fatalf("Error finding pkexec executable: %v", err)
		}
	}

	executablePath := os.Args[0];
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
