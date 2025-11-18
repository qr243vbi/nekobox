package main

import (
	"Core/gen"
	"context"
	"fmt"
	"os"
	"syscall"
	"time"
)

func (s *server) SetSystemDNS(in *gen.SetSystemDNSRequest, out *gen.EmptyResp) error {
	return nil
}

func runAdmin(_port *int, _debug *bool) (int, error) {
	return 0, nil
}

func isElevated() (bool, error) {
	return (os.Geteuid() == 0), nil
}

func (s *server) IsPrivileged(ctx context.Context, in *gen.EmptyReq) (*gen.IsPrivilegedResponse, error) {
	out := new(gen.IsPrivilegedResponse)
	out.HasPrivilege = (os.Geteuid() == 0)
	return out, nil
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

func restartAsAdmin() {
}
