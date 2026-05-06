package tun

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"net"
	"net/netip"
	"os"
	"syscall"
	"unsafe"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	N "github.com/sagernet/sing/common/network"
	"golang.org/x/net/route"
	"golang.org/x/sys/unix"
)

const IFHEADOffset = 4
const PacketOffset = IFHEADOffset

const (
	_TUNSIFHEAD = 0x80047460

	_TUNSIFMODE = 0x8004745e
	_TUNGIFNAME = 0x4020745d
	_TUNSIFPID  = 0x2000745f

	_SIOCGIFINFO_IN6 = 0xc048696c
	_SIOCSIFINFO_IN6 = 0xc048696d

	_ND6_IFF_AUTO_LINKLOCAL = 0x20
	_ND6_IFF_NO_DAD         = 0x100

	// NOTE: SIOCSxxx deprecated
	_SIOCAIFADDR_IN6       = 0x8088691b // netinet6/in6_var.h
	_IN6_IFF_NODAD         = 0x20       // netinet6/in6_var.h
	_ND6_INFINITE_LIFETIME = 0xFFFFFFFF // netinet6/nd6.h
)

var _ Tun = (*NativeTun)(nil)

type NativeTun struct {
	name    string
	tunFile *os.File

	fd  int
	mtu uint32
	unix.RawSockaddrInet6
	tunWriter N.VectorisedWriter

	inet4Address [4]byte
	inet6Address [16]byte

	routeCleanFns []func() error
}

func New(options Options) (Tun, error) {
	if len(options.Name) > unix.IFNAMSIZ-1 {
		return nil, E.New("tun name too long: ", options.Name)
	}

	// See if interface already exists
	if iface, _ := net.InterfaceByName(options.Name); iface != nil {
		if err := destoryIf(options.Name); err != nil {
			return nil, E.New("unable able to destory already existed interface: ", options.Name)
		}
	}

	tunFile, err := os.OpenFile("/dev/tun", unix.O_RDWR|unix.O_CLOEXEC, 0)
	if err != nil {
		return nil, E.New("unable able to open /dev/tun: ", err)
	}

	tun := &NativeTun{
		name:          options.Name,
		tunFile:       tunFile,
		fd:            int(tunFile.Fd()),
		mtu:           options.MTU,
		routeCleanFns: make([]func() error, 0),
	}

	var assignedName string
	if assignedName, err = fdevName(tun.tunFile); err != nil {
		tunFile.Close()
		destoryIf(assignedName)
		return nil, err
	}

	if err := enableIfHeadMode(tun.tunFile); err != nil {
		tun.tunFile.Close()
		destoryIf(assignedName)
		return nil, err
	}

	if err := setIfMode(tun.tunFile, syscall.IFF_POINTOPOINT|syscall.IFF_MULTICAST); err != nil {
		tun.tunFile.Close()
		destoryIf(assignedName)
		return nil, err
	}

	if err := disableLinkLocalV6(assignedName); err != nil {
		tun.tunFile.Close()
		destoryIf(assignedName)
		return nil, err
	}

	if len(options.Name) > 0 {
		if err := setIfName(assignedName, options.Name); err != nil {
			tun.tunFile.Close()
			destoryIf(assignedName)
			return nil, err
		}
	}

	if err := becomeCtrlProc(tun.tunFile); err != nil {
		tun.tunFile.Close()
		destoryIf(tun.name)
		return nil, err
	}

	err = unix.SetNonblock(tun.fd, true)
	if err != nil {
		tun.tunFile.Close()
		destoryIf(tun.name)
		return nil, err
	}

	// update if name here?
	if err := setMTU(tun.name, options.MTU); err != nil {
		tun.tunFile.Close()
		destoryIf(tun.name)
		return nil, err
	}

	if err := setIpV4(tun.name, options.Inet4Address); err != nil {
		tun.tunFile.Close()
		return nil, err
	}
	if len(options.Inet4Address) > 0 {
		tun.inet4Address = options.Inet4Address[0].Addr().As4()
	}

	if err := setIpV6(tun.name, options.Inet6Address); err != nil {
		tun.tunFile.Close()
		return nil, err
	}
	if len(options.Inet6Address) > 0 {
		tun.inet6Address = options.Inet6Address[0].Addr().As16()
	}

	// can work?
	var ok bool
	tun.tunWriter, ok = bufio.CreateVectorisedWriter(tun.tunFile)
	if !ok {
		panic("create vectorised writer")
	}

	// same as darwin
	if options.AutoRoute {
		var routeRanges []netip.Prefix
		routeRanges, _ = options.BuildAutoRouteRanges(false)
		for _, routeRange := range routeRanges {
			var fn func() error
			if routeRange.Addr().Is4() {
				fn, err = addRoute(routeRange, options.Inet4Address[0].Addr())
			} else {
				fn, err = addRoute(routeRange, options.Inet6Address[0].Addr())
			}
			if err != nil {
				return nil, E.Cause(err, "add route: ", routeRange)
			}
			tun.routeCleanFns = append(tun.routeCleanFns, fn)
		}
	}

	if err := ifUp(tun.name); err != nil {
		tun.tunFile.Close()
		return nil, err
	}

	return tun, nil
}

// Read implements Tun.
func (t *NativeTun) Read(p []byte) (n int, err error) {
	return t.tunFile.Read(p)
}

// Write implements Tun.
func (t *NativeTun) Write(p []byte) (n int, err error) {
	return t.tunFile.Write(p)
}

// Close implements Tun.
func (t *NativeTun) Close() error {

	for _, fn := range t.routeCleanFns {
		if err := fn(); err != nil {
			// TODO: deal with undeleted route?
			continue
		}
	}

	if err := t.tunFile.Close(); err != nil {
		return err
	}

	if err := destoryIf(t.name); err != nil {
		return err
	}
	return nil
}

var (
	packetHeader4 = [IFHEADOffset]byte{0x00, 0x00, 0x00, unix.AF_INET}
	packetHeader6 = [IFHEADOffset]byte{0x00, 0x00, 0x00, unix.AF_INET6}
)

// WriteVectorised implements Tun. work?
// buffers is full ip pkg without IFHEAD setted, before write add the 4 bytes header.
func (t *NativeTun) WriteVectorised(buffers []*buf.Buffer) error {
	var packetHeader []byte
	if buffers[0].Byte(0)>>4 == 4 {
		packetHeader = packetHeader4[:]
	} else {
		packetHeader = packetHeader6[:]
	}
	return t.tunWriter.WriteVectorised(append([]*buf.Buffer{buf.As(packetHeader)}, buffers...))
}

func operateOnFd(theFile *os.File, fn func(fd uintptr)) error {
	sysconn, err := theFile.SyscallConn()
	if err != nil {
		return fmt.Errorf("unable to find sysconn for tunfile: %s", err.Error())
	}
	err = sysconn.Control(fn)
	if err != nil {
		return fmt.Errorf("unable to control sysconn for tunfile: %s", err.Error())
	}
	return nil
}

// useSocket from tun_darwin
func useSocket(domain, typ, proto int, block func(socketFd int) error) error {
	socketFd, err := unix.Socket(domain, typ, proto)
	if err != nil {
		return err
	}
	defer unix.Close(socketFd)
	return block(socketFd)
}

func fdevName(theFile *os.File) (string, error) {
	ifreq := struct {
		Name [unix.IFNAMSIZ]byte
		_    [16]byte
	}{}

	var errno syscall.Errno
	operateOnFd(theFile, func(fd uintptr) {
		_, _, errno = unix.Syscall(unix.SYS_IOCTL, fd, _TUNGIFNAME, uintptr(unsafe.Pointer(&ifreq)))
	})

	if errno != 0 {
		return "", os.NewSyscallError("TUNGIFNAME", errno)
	}
	return unix.ByteSliceToString(ifreq.Name[:]), nil
}

// enableIfHeadMode https://man.freebsd.org/cgi/man.cgi?query=tun&sektion=4
func enableIfHeadMode(theFile *os.File) error {
	ifheadmode := 1

	var errno syscall.Errno
	operateOnFd(theFile, func(fd uintptr) {
		_, _, errno = unix.Syscall(unix.SYS_IOCTL, fd, _TUNSIFHEAD, uintptr(unsafe.Pointer(&ifheadmode)))
	})

	if errno != 0 {
		return os.NewSyscallError("TUNSIFHEAD", errno)
	}
	return nil
}

// setIfMode TUNSIFMODE
func setIfMode(theFile *os.File, mode int) error {
	ifflags := mode
	var errno syscall.Errno
	operateOnFd(theFile, func(fd uintptr) {
		_, _, errno = unix.Syscall(unix.SYS_IOCTL, fd, uintptr(_TUNSIFMODE), uintptr(unsafe.Pointer(&ifflags)))
	})

	if errno != 0 {
		return os.NewSyscallError("TUNSIFMODE", errno)
	}
	return nil
}

func disableLinkLocalV6(name string) error {
	// Disable link-local v6, not just because WireGuard doesn't do that anyway, but
	// also because there are serious races with attaching and detaching LLv6 addresses
	// in relation to interface lifetime within the FreeBSD kernel.

	// ND6 flag manipulation
	ndireq := struct {
		Name          [unix.IFNAMSIZ]byte
		Linkmtu       uint32
		Maxmtu        uint32
		Basereachable uint32
		Reachable     uint32
		Retrans       uint32
		Flags         uint32
		Recalctm      int
		Chlim         uint8
		Initialized   uint8
		Randomseed0   [8]byte
		Randomseed1   [8]byte
		Randomid      [8]byte
	}{}
	copy(ndireq.Name[:], name)

	return useSocket(unix.AF_INET6, unix.SOCK_DGRAM|unix.SOCK_CLOEXEC, 0, func(socketFd int) error {
		var errno syscall.Errno

		_, _, errno = unix.Syscall(unix.SYS_IOCTL, uintptr(socketFd), uintptr(_SIOCGIFINFO_IN6), uintptr(unsafe.Pointer(&ndireq)))
		if errno != 0 {
			return os.NewSyscallError("SIOCGIFINFO_IN6", errno)
		}

		ndireq.Flags = ndireq.Flags &^ _ND6_IFF_AUTO_LINKLOCAL
		ndireq.Flags = ndireq.Flags | _ND6_IFF_NO_DAD
		_, _, errno = unix.Syscall(unix.SYS_IOCTL, uintptr(socketFd), uintptr(_SIOCSIFINFO_IN6), uintptr(unsafe.Pointer(&ndireq)))
		if errno != 0 {
			return os.NewSyscallError("SIOCSIFINFO_IN6", errno)
		}
		return nil
	})

}

func setIfName(targetIfName, name string) error {
	var newnp [unix.IFNAMSIZ]byte
	copy(newnp[:], name)

	// Iface requests with a pointer
	ifr := struct {
		Name [unix.IFNAMSIZ]byte
		Data uintptr
		_    [16 - unsafe.Sizeof(uintptr(0))]byte
	}{}
	copy(ifr.Name[:], targetIfName)
	ifr.Data = uintptr(unsafe.Pointer(&newnp[0]))

	return useSocket(unix.AF_INET, unix.SOCK_DGRAM|unix.SOCK_CLOEXEC, 0, func(socketFd int) error {
		_, _, errno := unix.Syscall(unix.SYS_IOCTL, uintptr(socketFd), uintptr(unix.SIOCSIFNAME), uintptr(unsafe.Pointer(&ifr)))
		if errno != 0 {
			return os.NewSyscallError("SIOCSIFNAME", errno)
		}
		return nil
	})

}

func becomeCtrlProc(theFile *os.File) error {
	var errno syscall.Errno
	operateOnFd(theFile, func(fd uintptr) {
		_, _, errno = unix.Syscall(unix.SYS_IOCTL, fd, _TUNSIFPID, uintptr(0))
	})
	if errno != 0 {
		return os.NewSyscallError("TUNSIFPID", errno)
	}
	return nil
}

func setMTU(ifName string, n uint32) error {

	ifr := struct {
		Name [unix.IFNAMSIZ]byte
		MTU  uint32
		_    [12]byte
	}{}
	copy(ifr.Name[:], ifName)
	ifr.MTU = uint32(n)

	return useSocket(unix.AF_INET, unix.SOCK_DGRAM|unix.SOCK_CLOEXEC, 0, func(socketFd int) error {
		_, _, errno := unix.Syscall(unix.SYS_IOCTL, uintptr(socketFd), uintptr(unix.SIOCSIFMTU), uintptr(unsafe.Pointer(&ifr)))
		if errno != 0 {
			return os.NewSyscallError("SIOCSIFMTU", errno)
		}
		return nil
	})

}

// getMTU get mtu of interface
func getMTU(ifName string) (int, error) {

	ifr := struct {
		Name [unix.IFNAMSIZ]byte
		MTU  uint32
		_    [12]byte
	}{}
	copy(ifr.Name[:], ifName)

	if err := useSocket(unix.AF_INET, unix.SOCK_DGRAM|unix.SOCK_CLOEXEC, 0, func(socketFd int) error {
		_, _, errno := unix.Syscall(unix.SYS_IOCTL, uintptr(socketFd), uintptr(unix.SIOCGIFMTU), uintptr(unsafe.Pointer(&ifr)))
		if errno != 0 {
			return os.NewSyscallError("SIOCGIFMTU", errno)
		}
		return nil
	}); err == nil {
		return int(*(*int32)(unsafe.Pointer(&ifr.MTU))), nil
	} else {
		return 0, err
	}

}

// setIpV4 set v4 ip for specific interface, but the
// ip will be removed if the tun dev was cloed
func setIpV4(ifName string, addresses []netip.Prefix) error {

	if len(addresses) <= 0 {
		return nil
	}

	return useSocket(unix.AF_INET, unix.SOCK_DGRAM|unix.SOCK_CLOEXEC, 0, func(socketFd int) error {

		for _, address := range addresses {
			ifr := struct {
				Name      [unix.IFNAMSIZ]byte
				Addr      unix.RawSockaddrInet4
				BroadAddr unix.RawSockaddrInet4
				Mask      unix.RawSockaddrInet4
			}{
				Addr: unix.RawSockaddrInet4{
					Family: unix.AF_INET,
					Len:    unix.SizeofSockaddrInet4,
					Addr:   address.Addr().As4(),
				},
				BroadAddr: unix.RawSockaddrInet4{
					Family: unix.AF_INET,
					Len:    unix.SizeofSockaddrInet4,
					Addr:   broadAddr(address),
				},
				Mask: unix.RawSockaddrInet4{
					Family: unix.AF_INET,
					Len:    unix.SizeofSockaddrInet4,
					Addr:   mustParseSubnetMask4(address),
				},
			}
			copy(ifr.Name[:], ifName)

			_, _, errno := unix.Syscall(
				unix.SYS_IOCTL,
				uintptr(socketFd),
				uintptr(unix.SIOCAIFADDR),
				uintptr(unsafe.Pointer(&ifr)),
			)
			if errno != 0 {
				return os.NewSyscallError("SIOCAIFADDR", errno)
			}
		}

		return nil
	})

}

func setIpV6(ifName string, addresses []netip.Prefix) error {
	if len(addresses) <= 0 {
		return nil
	}

	return useSocket(unix.AF_INET6, unix.SOCK_DGRAM|unix.SOCK_CLOEXEC, 0, func(socketFd int) error {

		for _, address := range addresses {

			// netinet6/in6_var.h: struct in6_addrlifetime
			type addrLifetime6 struct {
				Expire    float64
				Preferred float64
				Vltime    uint32
				Pltime    uint32
			}

			// netinet6/in6_var.h: struct in6_aliasreq
			in6_ifreq := struct {
				Name [unix.IFNAMSIZ]byte
				Addr unix.RawSockaddrInet6
				Mask unix.RawSockaddrInet6
				// Dstaddr contain the destination address of the point-to-point interface
				Dstaddr  unix.RawSockaddrInet6
				Flags    uint32
				Lifetime addrLifetime6
				// Vhid     uint32
			}{
				Addr: unix.RawSockaddrInet6{
					Len:    unix.SizeofSockaddrInet6,
					Family: unix.AF_INET6,
					Addr:   address.Addr().As16(),
				},
				Mask: unix.RawSockaddrInet6{
					Len:    unix.SizeofSockaddrInet6,
					Family: unix.AF_INET6,
					Addr:   mustParseSubnetMask6(address),
				},
				Flags: _IN6_IFF_NODAD,
				Lifetime: addrLifetime6{
					Vltime: _ND6_INFINITE_LIFETIME,
					Pltime: _ND6_INFINITE_LIFETIME,
				}}
			copy(in6_ifreq.Name[:], []byte(ifName))

			if address.Bits() == 128 {
				in6_ifreq.Dstaddr = unix.RawSockaddrInet6{
					Len:    unix.SizeofSockaddrInet6,
					Family: unix.AF_INET6,
					Addr:   address.Addr().Next().As16(),
				}
			}

			_, _, errno := unix.Syscall(
				unix.SYS_IOCTL,
				uintptr(socketFd),
				uintptr(_SIOCAIFADDR_IN6),
				uintptr(unsafe.Pointer(&in6_ifreq)),
			)
			if errno != 0 {
				return os.NewSyscallError("SIOCAIFADDR_IN6", errno)
			}

		}

		return nil
	})

}

func ifUp(ifName string) error {

	ifrFlags := struct {
		Name  [unix.IFNAMSIZ]byte
		Flags uint16
	}{
		Flags: unix.IFF_UP | unix.IFF_RUNNING,
	}
	copy(ifrFlags.Name[:], ifName)

	return useSocket(unix.AF_INET, unix.SOCK_DGRAM|unix.SOCK_CLOEXEC, 0, func(socketFd int) error {
		_, _, errno := unix.Syscall(
			unix.SYS_IOCTL,
			uintptr(socketFd),
			uintptr(unix.SIOCSIFFLAGS),
			uintptr(unsafe.Pointer(&ifrFlags)),
		)
		if errno != 0 {
			return os.NewSyscallError("SIOCSIFFLAGS", errno)
		}

		return nil
	})

}

func destoryIf(name string) error {

	var ifr [32]byte
	copy(ifr[:], name)

	return useSocket(unix.AF_INET, unix.SOCK_DGRAM|unix.SOCK_CLOEXEC, 0, func(socketFd int) error {
		_, _, errno := unix.Syscall(unix.SYS_IOCTL, uintptr(socketFd), uintptr(unix.SIOCIFDESTROY), uintptr(unsafe.Pointer(&ifr[0])))
		if errno != 0 {
			return os.NewSyscallError("SIOCIFDESTROY", errno)
		}

		return nil
	})

}

func a4ToUint32(a4 [4]byte) uint32 {

	buffer := make([]byte, 4)
	for i, v := range a4 {
		buffer[i] = v
	}
	return binary.BigEndian.Uint32(buffer)
}

func uint32ToA4(val uint32) (a4 [4]byte) {
	buffer := new(bytes.Buffer)
	binary.Write(buffer, binary.BigEndian, val)
	var out [4]byte
	for i, v := range buffer.Bytes() {
		out[i] = v
	}
	return out
}

func mustParseSubnetMask4(address netip.Prefix) [4]byte {
	return netip.MustParseAddr(net.IP(net.CIDRMask(address.Bits(), address.Addr().BitLen())).String()).As4()
}

func mustParseSubnetMask6(address netip.Prefix) [16]byte {
	return netip.MustParseAddr(net.IP(net.CIDRMask(address.Bits(), address.Addr().BitLen())).String()).As16()
}

func networkAddr(address netip.Prefix) [4]byte {
	networkAddrUint32 := a4ToUint32(address.Addr().As4()) & a4ToUint32(mustParseSubnetMask4(address))
	return uint32ToA4(networkAddrUint32)
}

func broadAddr(address netip.Prefix) [4]byte {
	broadAddrUint32 := a4ToUint32(networkAddr(address)) | (^a4ToUint32(mustParseSubnetMask4(address)))
	return uint32ToA4(broadAddrUint32)
}

func addRoute(destination netip.Prefix, gateway netip.Addr) (func() error, error) {
	routeMessage := &route.RouteMessage{
		Type:    unix.RTM_ADD,
		Flags:   unix.RTF_UP | unix.RTF_STATIC | unix.RTF_GATEWAY,
		Version: unix.RTM_VERSION,
		ID:      uintptr(os.Getpid()),
		Seq:     1,
	}
	if gateway.Is4() {
		routeMessage.Addrs = []route.Addr{
			unix.RTAX_DST:     &route.Inet4Addr{IP: destination.Addr().As4()},
			unix.RTAX_NETMASK: &route.Inet4Addr{IP: netip.MustParseAddr(net.IP(net.CIDRMask(destination.Bits(), 32)).String()).As4()},
			unix.RTAX_GATEWAY: &route.Inet4Addr{IP: gateway.As4()},
		}
	} else {
		routeMessage.Addrs = []route.Addr{
			unix.RTAX_DST:     &route.Inet6Addr{IP: destination.Addr().As16()},
			unix.RTAX_NETMASK: &route.Inet6Addr{IP: netip.MustParseAddr(net.IP(net.CIDRMask(destination.Bits(), 128)).String()).As16()},
			unix.RTAX_GATEWAY: &route.Inet6Addr{IP: gateway.As16()},
		}
	}
	request, err := routeMessage.Marshal()
	if err != nil {
		return nil, err
	}

	if err := useSocket(unix.AF_ROUTE, unix.SOCK_RAW, 0, func(socketFd int) error {
		return common.Error(unix.Write(socketFd, request))
	}); err != nil {
		return nil, err
	}

	// for cleanup
	return func() error {
		routeMessage.Type = unix.RTM_DELETE
		desc := fmt.Sprintf("to %s via %s", destination.String(), gateway.String())

		request, err := routeMessage.Marshal()
		if err != nil {
			return E.New("route message marshal error: ", err)
		}
		err = useSocket(unix.AF_ROUTE, unix.SOCK_RAW, 0, func(socketFd int) error {
			return common.Error(unix.Write(socketFd, request))
		})
		if err != nil {
			return E.New("unable to delete route ", desc, ": ", err)
		}
		return nil
	}, nil
}
