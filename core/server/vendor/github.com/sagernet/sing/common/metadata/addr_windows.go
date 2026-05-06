package metadata

import (
	"encoding/binary"
	"net/netip"
	"unsafe"

	"golang.org/x/sys/windows"
)

func AddrPortFromSockaddr(sa windows.Sockaddr) netip.AddrPort {
	switch addr := sa.(type) {
	case *windows.SockaddrInet4:
		return netip.AddrPortFrom(netip.AddrFrom4(addr.Addr), uint16(addr.Port))
	case *windows.SockaddrInet6:
		return netip.AddrPortFrom(netip.AddrFrom16(addr.Addr), uint16(addr.Port))
	default:
		return netip.AddrPort{}
	}
}

func AddrPortToSockaddr(addrPort netip.AddrPort) windows.Sockaddr {
	if addrPort.Addr().Is4() {
		return &windows.SockaddrInet4{
			Port: int(addrPort.Port()),
			Addr: addrPort.Addr().As4(),
		}
	} else {
		return &windows.SockaddrInet6{
			Port: int(addrPort.Port()),
			Addr: addrPort.Addr().As16(),
		}
	}
}

func AddrPortFromRawSockaddr(sa *windows.RawSockaddr) netip.AddrPort {
	switch sa.Family {
	case windows.AF_INET:
		sa4 := (*windows.RawSockaddrInet4)(unsafe.Pointer(sa))
		return netip.AddrPortFrom(netip.AddrFrom4(sa4.Addr), sa4.Port)
	case windows.AF_INET6:
		sa6 := (*windows.RawSockaddrInet6)(unsafe.Pointer(sa))
		return netip.AddrPortFrom(netip.AddrFrom16(sa6.Addr), sa6.Port)
	default:
		return netip.AddrPort{}
	}
}

func AddrPortToRawSockaddr(addrPort netip.AddrPort, forceInet6 bool) (name unsafe.Pointer, nameLen int32) {
	if addrPort.Addr().Is4() && !forceInet6 {
		sa := windows.RawSockaddrInet4{
			Family: windows.AF_INET,
			Addr:   addrPort.Addr().As4(),
		}
		binary.BigEndian.PutUint16((*[2]byte)(unsafe.Pointer(&sa.Port))[:], addrPort.Port())
		name = unsafe.Pointer(&sa)
		nameLen = int32(unsafe.Sizeof(sa))
	} else {
		sa := windows.RawSockaddrInet6{
			Family: windows.AF_INET6,
			Addr:   addrPort.Addr().As16(),
		}
		binary.BigEndian.PutUint16((*[2]byte)(unsafe.Pointer(&sa.Port))[:], addrPort.Port())
		name = unsafe.Pointer(&sa)
		nameLen = int32(unsafe.Sizeof(sa))
	}
	return
}
