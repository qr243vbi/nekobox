//go:build aix || darwin

package netx

import (
	"os"
	"syscall"

	"golang.org/x/sys/unix"
)

// Copied and modified from src/net/sys_cloexec.go

// Wrapper around the socket system call that marks the returned file
// descriptor as nonblocking and close-on-exec.
func sysSocket(family, sotype, proto int) (int, error) {
	// See ../syscall/exec_unix.go for description of ForkLock.
	syscall.ForkLock.RLock()
	s, err := unix.Socket(family, sotype, proto)
	if err == nil {
		unix.CloseOnExec(s)
	}
	syscall.ForkLock.RUnlock()
	if err != nil {
		return -1, os.NewSyscallError("socket", err)
	}
	if err = unix.SetNonblock(s, true); err != nil {
		unix.Close(s)
		return -1, os.NewSyscallError("setnonblock", err)
	}
	return s, nil
}
