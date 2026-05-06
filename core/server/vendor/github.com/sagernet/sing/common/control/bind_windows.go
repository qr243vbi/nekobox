package control

import (
	"encoding/binary"
	"os"
	"syscall"
	"unsafe"

	M "github.com/sagernet/sing/common/metadata"
)

func bindToInterface(conn syscall.RawConn, network string, address string, finder InterfaceFinder, interfaceName string, interfaceIndex int, preferInterfaceName bool) error {
	return Raw(conn, func(fd uintptr) error {
		if interfaceIndex == -1 {
			if finder == nil {
				return os.ErrInvalid
			}
			iif, err := finder.ByName(interfaceName)
			if err != nil {
				return err
			}
			interfaceIndex = iif.Index
		}
		handle := syscall.Handle(fd)
		if M.ParseSocksaddr(address).AddrString() == "" {
			err := bind4(handle, interfaceIndex)
			if err != nil {
				return err
			}
			// try bind ipv6, if failed, ignore. it's a workaround for windows disable interface ipv6
			bind6(handle, interfaceIndex)
			return nil
		}
		switch network {
		case "tcp4", "udp4", "ip4":
			return bind4(handle, interfaceIndex)
		default:
			return bind6(handle, interfaceIndex)
		}
	})
}

const (
	IP_UNICAST_IF   = 31
	IPV6_UNICAST_IF = 31
)

func bind4(handle syscall.Handle, ifaceIdx int) error {
	var bytes [4]byte
	binary.BigEndian.PutUint32(bytes[:], uint32(ifaceIdx))
	idx := *(*uint32)(unsafe.Pointer(&bytes[0]))
	return syscall.SetsockoptInt(handle, syscall.IPPROTO_IP, IP_UNICAST_IF, int(idx))
}

func bind6(handle syscall.Handle, ifaceIdx int) error {
	return syscall.SetsockoptInt(handle, syscall.IPPROTO_IPV6, IPV6_UNICAST_IF, ifaceIdx)
}
