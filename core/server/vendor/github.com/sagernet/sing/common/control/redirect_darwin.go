package control

import (
	"encoding/binary"
	"net"
	"net/netip"
	"syscall"
	"unsafe"

	M "github.com/sagernet/sing/common/metadata"

	"golang.org/x/sys/unix"
)

const (
	PF_OUT      = 0x2
	DIOCNATLOOK = 0xc0544417
)

func GetOriginalDestination(conn net.Conn) (netip.AddrPort, error) {
	pfFd, err := syscall.Open("/dev/pf", 0, syscall.O_RDONLY)
	if err != nil {
		return netip.AddrPort{}, err
	}
	defer syscall.Close(pfFd)
	nl := struct {
		saddr, daddr, rsaddr, rdaddr       [16]byte
		sxport, dxport, rsxport, rdxport   [4]byte
		af, proto, protoVariant, direction uint8
	}{
		af:        syscall.AF_INET,
		proto:     syscall.IPPROTO_TCP,
		direction: PF_OUT,
	}
	localAddr := M.SocksaddrFromNet(conn.LocalAddr()).Unwrap()
	removeAddr := M.SocksaddrFromNet(conn.RemoteAddr()).Unwrap()
	if localAddr.IsIPv4() {
		copy(nl.saddr[:net.IPv4len], removeAddr.Addr.AsSlice())
		copy(nl.daddr[:net.IPv4len], localAddr.Addr.AsSlice())
		nl.af = syscall.AF_INET
	} else {
		copy(nl.saddr[:], removeAddr.Addr.AsSlice())
		copy(nl.daddr[:], localAddr.Addr.AsSlice())
		nl.af = syscall.AF_INET6
	}
	binary.BigEndian.PutUint16(nl.sxport[:], removeAddr.Port)
	binary.BigEndian.PutUint16(nl.dxport[:], localAddr.Port)
	if _, _, errno := unix.Syscall(syscall.SYS_IOCTL, uintptr(pfFd), DIOCNATLOOK, uintptr(unsafe.Pointer(&nl))); errno != 0 {
		return netip.AddrPort{}, errno
	}
	var address netip.Addr
	if nl.af == unix.AF_INET {
		address = M.AddrFromIP(nl.rdaddr[:net.IPv4len])
	} else {
		address = netip.AddrFrom16(nl.rdaddr)
	}
	return netip.AddrPortFrom(address, binary.BigEndian.Uint16(nl.rdxport[:])), nil
}
