package control

import (
	"os"
	"sync/atomic"
	"syscall"

	E "github.com/sagernet/sing/common/exceptions"

	"golang.org/x/sys/unix"
)

var ifIndexDisabled atomic.Bool

func bindToInterface(conn syscall.RawConn, network string, address string, finder InterfaceFinder, interfaceName string, interfaceIndex int, preferInterfaceName bool) error {
	return Raw(conn, func(fd uintptr) error {
		if !preferInterfaceName && !ifIndexDisabled.Load() {
			if interfaceIndex == -1 {
				if interfaceName == "" {
					return os.ErrInvalid
				}
				iif, err := finder.ByName(interfaceName)
				if err != nil {
					return err
				}
				interfaceIndex = iif.Index
			}
			err := unix.SetsockoptInt(int(fd), unix.SOL_SOCKET, unix.SO_BINDTOIFINDEX, interfaceIndex)
			if err == nil {
				return nil
			} else if E.IsMulti(err, unix.ENOPROTOOPT, unix.EINVAL) {
				ifIndexDisabled.Store(true)
			} else {
				return err
			}
		}
		if interfaceName == "" {
			return os.ErrInvalid
		}
		return unix.BindToDevice(int(fd), interfaceName)
	})
}
