package control

import (
	"syscall"
	"time"
	_ "unsafe"

	E "github.com/sagernet/sing/common/exceptions"
	N "github.com/sagernet/sing/common/network"

	"golang.org/x/sys/unix"
)

func SetKeepAlivePeriod(idle time.Duration, interval time.Duration) Func {
	return func(network, address string, conn syscall.RawConn) error {
		if N.NetworkName(network) != N.NetworkTCP {
			return nil
		}
		return Raw(conn, func(fd uintptr) error {
			return E.Errors(
				unix.SetsockoptInt(int(fd), unix.IPPROTO_TCP, unix.TCP_KEEPIDLE, int(roundDurationUp(idle, time.Second))),
				unix.SetsockoptInt(int(fd), unix.IPPROTO_TCP, unix.TCP_KEEPINTVL, int(roundDurationUp(interval, time.Second))),
			)
		})
	}
}

func roundDurationUp(d time.Duration, to time.Duration) time.Duration {
	return (d + to - 1) / to
}
