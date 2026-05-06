package control

import (
	"syscall"

	E "github.com/sagernet/sing/common/exceptions"
	N "github.com/sagernet/sing/common/network"

	"golang.org/x/sys/unix"
)

func BindAddressNoPort() Func {
	return func(network, address string, conn syscall.RawConn) error {
		if N.NetworkName(network) != N.NetworkTCP {
			return nil
		}
		return Raw(conn, func(fd uintptr) error {
			err := unix.SetsockoptInt(int(fd), unix.SOL_IP, unix.IP_BIND_ADDRESS_NO_PORT, 1)
			if err != nil && E.IsMulti(err, unix.ENOPROTOOPT, unix.EINVAL) {
				return nil
			}
			return err
		})
	}
}
