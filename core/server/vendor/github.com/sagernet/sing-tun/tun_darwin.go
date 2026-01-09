package tun

import (
	"errors"
	"fmt"
	"net"
	"net/netip"
	"os"
	"syscall"
	"unsafe"

	"github.com/sagernet/sing-tun/internal/gtcpip/header"
	"github.com/sagernet/sing-tun/internal/rawfile_darwin"
	"github.com/sagernet/sing-tun/internal/stopfd_darwin"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/shell"

	"golang.org/x/net/route"
	"golang.org/x/sys/unix"
)

var _ DarwinTUN = (*NativeTun)(nil)

const PacketOffset = 4

type NativeTun struct {
	tunFd               int
	tunFile             *os.File
	batchSize           int
	iovecs              []iovecBuffer
	iovecsOutput        []iovecBuffer
	iovecsOutputDefault []unix.Iovec
	msgHdrs             []rawfile.MsgHdrX
	msgHdrsOutput       []rawfile.MsgHdrX
	buffers             []*buf.Buffer
	stopFd              stopfd.StopFD
	options             Options
	inet4Address        [4]byte
	inet6Address        [16]byte
	routeSet            bool
	sendMsgX            bool
}

type iovecBuffer struct {
	mtu    int
	buffer *buf.Buffer
	iovecs []unix.Iovec
}

func newIovecBuffer(mtu int) iovecBuffer {
	return iovecBuffer{
		mtu:    mtu,
		iovecs: make([]unix.Iovec, 2),
	}
}

func (b *iovecBuffer) nextIovecs() []unix.Iovec {
	if b.iovecs[0].Len == 0 {
		headBuffer := make([]byte, PacketOffset)
		b.iovecs[0].Base = &headBuffer[0]
		b.iovecs[0].SetLen(PacketOffset)
	}
	if b.buffer == nil {
		b.buffer = buf.NewSize(b.mtu)
		b.iovecs[1] = b.buffer.Iovec(b.buffer.Cap())
	}
	return b.iovecs
}

func (b *iovecBuffer) nextIovecsOutput(buffer *buf.Buffer) []unix.Iovec {
	switch header.IPVersion(buffer.Bytes()) {
	case header.IPv4Version:
		b.iovecs[0] = packetHeaderVec4
	case header.IPv6Version:
		b.iovecs[0] = packetHeaderVec6
	}
	b.iovecs[1] = buffer.Iovec(buffer.Len())
	return b.iovecs
}

func (t *NativeTun) Name() (string, error) {
	return unix.GetsockoptString(
		int(t.tunFile.Fd()),
		2, /* #define SYSPROTO_CONTROL 2 */
		2, /* #define UTUN_OPT_IFNAME 2 */
	)
}

func New(options Options) (Tun, error) {
	var tunFd int
	batchSize := ((512 * 1024) / int(options.MTU)) + 1
	if options.FileDescriptor == 0 {
		ifIndex := -1
		_, err := fmt.Sscanf(options.Name, "utun%d", &ifIndex)
		if err != nil {
			return nil, E.New("bad tun name: ", options.Name)
		}

		tunFd, err = unix.Socket(unix.AF_SYSTEM, unix.SOCK_DGRAM, 2)
		if err != nil {
			return nil, err
		}

		err = create(tunFd, ifIndex, options.Name, options)
		if err != nil {
			unix.Close(tunFd)
			return nil, err
		}
		err = configure(tunFd, options.EXP_MultiPendingPackets, batchSize)
		if err != nil {
			unix.Close(tunFd)
			return nil, err
		}
	} else {
		tunFd = options.FileDescriptor
		err := configure(tunFd, options.EXP_MultiPendingPackets, batchSize)
		if err != nil {
			return nil, err
		}
	}
	nativeTun := &NativeTun{
		tunFd:         tunFd,
		tunFile:       os.NewFile(uintptr(tunFd), "utun"),
		options:       options,
		batchSize:     batchSize,
		iovecs:        make([]iovecBuffer, batchSize),
		iovecsOutput:  make([]iovecBuffer, batchSize),
		msgHdrs:       make([]rawfile.MsgHdrX, batchSize),
		msgHdrsOutput: make([]rawfile.MsgHdrX, batchSize),
		stopFd:        common.Must1(stopfd.New()),
		sendMsgX:      options.EXP_SendMsgX,
	}
	for i := 0; i < batchSize; i++ {
		nativeTun.iovecs[i] = newIovecBuffer(int(options.MTU))
		nativeTun.iovecsOutput[i] = newIovecBuffer(int(options.MTU))
	}
	if len(options.Inet4Address) > 0 {
		nativeTun.inet4Address = options.Inet4Address[0].Addr().As4()
	}
	if len(options.Inet6Address) > 0 {
		nativeTun.inet6Address = options.Inet6Address[0].Addr().As16()
	}
	return nativeTun, nil
}

func (t *NativeTun) Start() error {
	t.options.InterfaceMonitor.RegisterMyInterface(t.options.Name)
	return t.setRoutes()
}

func (t *NativeTun) Close() error {
	defer flushDNSCache()
	return E.Errors(t.unsetRoutes(), t.tunFile.Close())
}

func (t *NativeTun) Read(p []byte) (n int, err error) {
	return t.tunFile.Read(p)
}

func (t *NativeTun) Write(p []byte) (n int, err error) {
	return t.tunFile.Write(p)
}

var (
	packetHeader4    = []byte{0x00, 0x00, 0x00, unix.AF_INET}
	packetHeader6    = []byte{0x00, 0x00, 0x00, unix.AF_INET6}
	packetHeaderVec4 = unix.Iovec{Base: &packetHeader4[0]}
	packetHeaderVec6 = unix.Iovec{Base: &packetHeader6[0]}
)

func init() {
	packetHeaderVec4.SetLen(4)
	packetHeaderVec6.SetLen(4)
}

const utunControlName = "com.apple.net.utun_control"

const (
	SIOCAIFADDR_IN6       = 2155899162 // netinet6/in6_var.h
	IN6_IFF_NODAD         = 0x0020     // netinet6/in6_var.h
	IN6_IFF_SECURED       = 0x0400     // netinet6/in6_var.h
	ND6_INFINITE_LIFETIME = 0xFFFFFFFF // netinet6/nd6.h
)

type ifAliasReq struct {
	Name    [unix.IFNAMSIZ]byte
	Addr    unix.RawSockaddrInet4
	Dstaddr unix.RawSockaddrInet4
	Mask    unix.RawSockaddrInet4
}

type ifAliasReq6 struct {
	Name     [16]byte
	Addr     unix.RawSockaddrInet6
	Dstaddr  unix.RawSockaddrInet6
	Mask     unix.RawSockaddrInet6
	Flags    uint32
	Lifetime addrLifetime6
}

type addrLifetime6 struct {
	Expire    float64
	Preferred float64
	Vltime    uint32
	Pltime    uint32
}

func create(tunFd int, ifIndex int, name string, options Options) error {
	ctlInfo := &unix.CtlInfo{}
	copy(ctlInfo.Name[:], utunControlName)
	err := unix.IoctlCtlInfo(tunFd, ctlInfo)
	if err != nil {
		return os.NewSyscallError("IoctlCtlInfo", err)
	}

	err = unix.Connect(tunFd, &unix.SockaddrCtl{
		ID:   ctlInfo.Id,
		Unit: uint32(ifIndex) + 1,
	})
	if err != nil {
		return os.NewSyscallError("Connect", err)
	}

	err = useSocket(unix.AF_INET, unix.SOCK_DGRAM, 0, func(socketFd int) error {
		var ifr unix.IfreqMTU
		copy(ifr.Name[:], name)
		ifr.MTU = int32(options.MTU)
		return unix.IoctlSetIfreqMTU(socketFd, &ifr)
	})
	if err != nil {
		return os.NewSyscallError("IoctlSetIfreqMTU", err)
	}
	if len(options.Inet4Address) > 0 {
		for _, address := range options.Inet4Address {
			ifReq := ifAliasReq{
				Addr: unix.RawSockaddrInet4{
					Len:    unix.SizeofSockaddrInet4,
					Family: unix.AF_INET,
					Addr:   address.Addr().As4(),
				},
				Dstaddr: unix.RawSockaddrInet4{
					Len:    unix.SizeofSockaddrInet4,
					Family: unix.AF_INET,
					Addr:   address.Addr().As4(),
				},
				Mask: unix.RawSockaddrInet4{
					Len:    unix.SizeofSockaddrInet4,
					Family: unix.AF_INET,
					Addr:   netip.MustParseAddr(net.IP(net.CIDRMask(address.Bits(), 32)).String()).As4(),
				},
			}
			copy(ifReq.Name[:], name)
			err = useSocket(unix.AF_INET, unix.SOCK_DGRAM, 0, func(socketFd int) error {
				if _, _, errno := unix.Syscall(
					syscall.SYS_IOCTL,
					uintptr(socketFd),
					uintptr(unix.SIOCAIFADDR),
					uintptr(unsafe.Pointer(&ifReq)),
				); errno != 0 {
					return os.NewSyscallError("SIOCAIFADDR", errno)
				}
				return nil
			})
			if err != nil {
				return err
			}
		}
	}
	if len(options.Inet6Address) > 0 {
		for _, address := range options.Inet6Address {
			ifReq6 := ifAliasReq6{
				Addr: unix.RawSockaddrInet6{
					Len:    unix.SizeofSockaddrInet6,
					Family: unix.AF_INET6,
					Addr:   address.Addr().As16(),
				},
				Mask: unix.RawSockaddrInet6{
					Len:    unix.SizeofSockaddrInet6,
					Family: unix.AF_INET6,
					Addr:   netip.MustParseAddr(net.IP(net.CIDRMask(address.Bits(), 128)).String()).As16(),
				},
				Flags: IN6_IFF_NODAD | IN6_IFF_SECURED,
				Lifetime: addrLifetime6{
					Vltime: ND6_INFINITE_LIFETIME,
					Pltime: ND6_INFINITE_LIFETIME,
				},
			}
			if address.Bits() == 128 {
				ifReq6.Dstaddr = unix.RawSockaddrInet6{
					Len:    unix.SizeofSockaddrInet6,
					Family: unix.AF_INET6,
					Addr:   address.Addr().Next().As16(),
				}
			}
			copy(ifReq6.Name[:], name)
			err = useSocket(unix.AF_INET6, unix.SOCK_DGRAM, 0, func(socketFd int) error {
				if _, _, errno := unix.Syscall(
					syscall.SYS_IOCTL,
					uintptr(socketFd),
					uintptr(SIOCAIFADDR_IN6),
					uintptr(unsafe.Pointer(&ifReq6)),
				); errno != 0 {
					return os.NewSyscallError("SIOCAIFADDR_IN6", errno)
				}
				return nil
			})
			if err != nil {
				return err
			}
		}
	}
	return nil
}

func configure(tunFd int, multiPendingPackets bool, batchSize int) error {
	err := unix.SetNonblock(tunFd, true)
	if err != nil {
		return os.NewSyscallError("SetNonblock", err)
	}
	if multiPendingPackets {
		const UTUN_OPT_MAX_PENDING_PACKETS = 16
		err = unix.SetsockoptInt(tunFd, 2, UTUN_OPT_MAX_PENDING_PACKETS, batchSize)
		if err != nil {
			return os.NewSyscallError("SetsockoptInt UTUN_OPT_MAX_PENDING_PACKETS", err)
		}
	}
	return nil
}

func (t *NativeTun) BatchRead() ([]*buf.Buffer, error) {
	for i := 0; i < t.batchSize; i++ {
		iovecs := t.iovecs[i].nextIovecs()
		// Cannot clear only the length field. Older versions of the darwin kernel will check whether other data is empty.
		// https://github.com/Darm64/XNU/blob/xnu-2782.40.9/bsd/kern/uipc_syscalls.c#L2026-L2048
		t.msgHdrs[i] = rawfile.MsgHdrX{}
		t.msgHdrs[i].Msg.Iov = &iovecs[0]
		t.msgHdrs[i].Msg.Iovlen = 2
	}
	n, errno := rawfile.BlockingRecvMMsgUntilStopped(t.stopFd.ReadFD, t.tunFd, t.msgHdrs)
	if errno != 0 {
		for k := 0; k < n; k++ {
			t.iovecs[k].buffer.Release()
			t.iovecs[k].buffer = nil
		}
		t.buffers = t.buffers[:0]
		return nil, errno
	}
	if n < 1 {
		return nil, nil
	}
	buffers := t.buffers
	for k := 0; k < n; k++ {
		buffer := t.iovecs[k].buffer
		t.iovecs[k].buffer = nil
		buffer.Truncate(int(t.msgHdrs[k].DataLen) - PacketOffset)
		buffers = append(buffers, buffer)
	}
	t.buffers = buffers[:0]
	return buffers, nil
}

func (t *NativeTun) BatchWrite(buffers []*buf.Buffer) error {
	if !t.sendMsgX {
		for i, buffer := range buffers {
			t.iovecsOutput[i].nextIovecsOutput(buffer)
		}
		for i := range buffers {
			errno := rawfile.NonBlockingWriteIovec(t.tunFd, t.iovecsOutput[i].iovecs)
			if errno != 0 {
				return errno
			}
		}
	} else {
		for i, buffer := range buffers {
			iovecs := t.iovecsOutput[i].nextIovecsOutput(buffer)
			t.msgHdrsOutput[i] = rawfile.MsgHdrX{}
			t.msgHdrsOutput[i].Msg.Iov = &iovecs[0]
			t.msgHdrsOutput[i].Msg.Iovlen = 2
		}
		var n int
		for n != len(buffers) {
			sent, errno := rawfile.NonBlockingSendMMsg(t.tunFd, t.msgHdrsOutput[n:len(buffers)])
			if errno != 0 {
				return errno
			}
			n += sent
		}
	}
	return nil
}

func (t *NativeTun) TXChecksumOffload() bool {
	return false
}

func (t *NativeTun) UpdateRouteOptions(tunOptions Options) error {
	err := t.unsetRoutes()
	if err != nil {
		return err
	}
	t.options = tunOptions
	return t.setRoutes()
}

func (t *NativeTun) setRoutes() error {
	if t.options.FileDescriptor == 0 {
		routeRanges, err := t.options.BuildAutoRouteRanges(false)
		if err != nil {
			return err
		}
		if len(routeRanges) > 0 {
			gateway4, gateway6 := t.options.Inet4GatewayAddr(), t.options.Inet6GatewayAddr()
			for _, destination := range routeRanges {
				var gateway netip.Addr
				if destination.Addr().Is4() {
					gateway = gateway4
				} else {
					gateway = gateway6
				}
				var interfaceIndex int
				if t.options.InterfaceScope {
					iff, err := t.options.InterfaceFinder.ByName(t.options.Name)
					if err != nil {
						return err
					}
					interfaceIndex = iff.Index
				}
				err = execRoute(unix.RTM_ADD, t.options.InterfaceScope, interfaceIndex, destination, gateway)
				if err != nil {
					if errors.Is(err, unix.EEXIST) {
						err = execRoute(unix.RTM_DELETE, false, 0, destination, gateway)
						if err != nil {
							return E.Cause(err, "remove existing route: ", destination)
						}
						err = execRoute(unix.RTM_ADD, t.options.InterfaceScope, interfaceIndex, destination, gateway)
						if err != nil {
							return E.Cause(err, "re-add route: ", destination)
						}
					} else {
						return E.Cause(err, "add route: ", destination)
					}
				}
			}
			flushDNSCache()
			t.routeSet = true
		}
	}
	return nil
}

func (t *NativeTun) unsetRoutes() error {
	if !t.routeSet {
		return nil
	}
	routeRanges, err := t.options.BuildAutoRouteRanges(false)
	if err != nil {
		return err
	}
	gateway4, gateway6 := t.options.Inet4GatewayAddr(), t.options.Inet6GatewayAddr()
	for _, destination := range routeRanges {
		var gateway netip.Addr
		if destination.Addr().Is4() {
			gateway = gateway4
		} else {
			gateway = gateway6
		}
		err = execRoute(unix.RTM_DELETE, false, 0, destination, gateway)
		if err != nil {
			err = E.Errors(err, E.Cause(err, "delete route: ", destination))
		}
	}
	return err
}

func useSocket(domain, typ, proto int, block func(socketFd int) error) error {
	socketFd, err := unix.Socket(domain, typ, proto)
	if err != nil {
		return err
	}
	defer unix.Close(socketFd)
	return block(socketFd)
}

func execRoute(rtmType int, interfaceScope bool, interfaceIndex int, destination netip.Prefix, gateway netip.Addr) error {
	routeMessage := route.RouteMessage{
		Type:    rtmType,
		Version: unix.RTM_VERSION,
		Flags:   unix.RTF_STATIC | unix.RTF_GATEWAY,
		Seq:     1,
	}
	if rtmType == unix.RTM_ADD {
		routeMessage.Flags |= unix.RTF_UP
		if interfaceScope {
			routeMessage.Flags |= unix.RTF_IFSCOPE
			routeMessage.Index = interfaceIndex
		}
	}
	if gateway.Is4() {
		routeMessage.Addrs = []route.Addr{
			syscall.RTAX_DST:     &route.Inet4Addr{IP: destination.Addr().As4()},
			syscall.RTAX_NETMASK: &route.Inet4Addr{IP: netip.MustParseAddr(net.IP(net.CIDRMask(destination.Bits(), 32)).String()).As4()},
			syscall.RTAX_GATEWAY: &route.Inet4Addr{IP: gateway.As4()},
		}
	} else {
		routeMessage.Addrs = []route.Addr{
			syscall.RTAX_DST:     &route.Inet6Addr{IP: destination.Addr().As16()},
			syscall.RTAX_NETMASK: &route.Inet6Addr{IP: netip.MustParseAddr(net.IP(net.CIDRMask(destination.Bits(), 128)).String()).As16()},
			syscall.RTAX_GATEWAY: &route.Inet6Addr{IP: gateway.As16()},
		}
	}
	request, err := routeMessage.Marshal()
	if err != nil {
		return err
	}
	return useSocket(unix.AF_ROUTE, unix.SOCK_RAW, 0, func(socketFd int) error {
		return common.Error(unix.Write(socketFd, request))
	})
}

func flushDNSCache() {
	go shell.Exec("dscacheutil", "-flushcache").Run()
}
