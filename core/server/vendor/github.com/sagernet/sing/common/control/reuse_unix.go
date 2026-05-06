//go:build darwin || dragonfly || freebsd || linux || netbsd || openbsd || solaris

package control

import (
	"syscall"

	E "github.com/sagernet/sing/common/exceptions"

	"golang.org/x/sys/unix"
)

func ReuseAddr() Func {
	return func(network, address string, conn syscall.RawConn) error {
		return Raw(conn, func(fd uintptr) error {
			return E.Errors(
				unix.SetsockoptInt(int(fd), unix.SOL_SOCKET, unix.SO_REUSEADDR, 1),
				unix.SetsockoptInt(int(fd), unix.SOL_SOCKET, unix.SO_REUSEPORT, 1),
			)
		})
	}
}
