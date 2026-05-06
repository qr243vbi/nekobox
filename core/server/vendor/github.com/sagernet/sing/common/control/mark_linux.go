package control

import (
	"syscall"
)

func RoutingMark(mark uint32) Func {
	return func(network, address string, conn syscall.RawConn) error {
		return Raw(conn, func(fd uintptr) error {
			return syscall.SetsockoptInt(int(fd), syscall.SOL_SOCKET, syscall.SO_MARK, int(mark))
		})
	}
}
