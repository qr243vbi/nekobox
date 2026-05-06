//go:build dragonfly || freebsd || linux || netbsd || openbsd || solaris

package netx

import (
	"os"

	"golang.org/x/sys/unix"
)

// Copied and modified from src/net/sock_cloexec.go
//
// Dropped src/net/sock_cloexec_solaris.go because it requires an unreasonable amount of effort
// for seemingly little gain of supporting older kernels.

// Wrapper around the socket system call that marks the returned file
// descriptor as nonblocking and close-on-exec.
func sysSocket(family, sotype, proto int) (int, error) {
	s, err := unix.Socket(family, sotype|unix.SOCK_NONBLOCK|unix.SOCK_CLOEXEC, proto)
	if err != nil {
		return -1, os.NewSyscallError("socket", err)
	}
	return s, nil
}
