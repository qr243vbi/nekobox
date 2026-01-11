package metadata

import (
	"net"
	"net/netip"
	"strconv"
	"unsafe"

	"github.com/sagernet/sing/common/debug"
)

type Socksaddr struct {
	Addr netip.Addr
	Port uint16
	Fqdn string
}

func (ap Socksaddr) Network() string {
	return "socks"
}

func (ap Socksaddr) IsIP() bool {
	return ap.Addr.IsValid()
}

func (ap Socksaddr) IsIPv4() bool {
	return ap.Addr.Is4()
}

func (ap Socksaddr) IsIPv6() bool {
	return ap.Addr.Is6()
}

func (ap Socksaddr) Unwrap() Socksaddr {
	if ap.Addr.Is4In6() {
		return Socksaddr{
			Addr: netip.AddrFrom4(ap.Addr.As4()),
			Port: ap.Port,
		}
	}
	return ap
}

func (ap Socksaddr) IsFqdn() bool {
	return IsDomainName(ap.Fqdn)
}

func (ap Socksaddr) IsValid() bool {
	return ap.IsIP() || ap.IsFqdn()
}

func (ap Socksaddr) AddrString() string {
	if debug.Enabled {
		ap.CheckBadAddr()
	}
	if ap.Addr.IsValid() {
		return ap.Addr.String()
	} else {
		return ap.Fqdn
	}
}

func (ap Socksaddr) IPAddr() *net.IPAddr {
	if debug.Enabled {
		ap.CheckBadAddr()
	}
	return &net.IPAddr{
		IP:   ap.Addr.AsSlice(),
		Zone: ap.Addr.Zone(),
	}
}

func (ap Socksaddr) TCPAddr() *net.TCPAddr {
	if debug.Enabled {
		ap.CheckBadAddr()
	}
	return &net.TCPAddr{
		IP:   ap.Addr.AsSlice(),
		Port: int(ap.Port),
		Zone: ap.Addr.Zone(),
	}
}

func (ap Socksaddr) UDPAddr() *net.UDPAddr {
	if debug.Enabled {
		ap.CheckBadAddr()
	}
	return &net.UDPAddr{
		IP:   ap.Addr.AsSlice(),
		Port: int(ap.Port),
		Zone: ap.Addr.Zone(),
	}
}

func (ap Socksaddr) AddrPort() netip.AddrPort {
	if debug.Enabled {
		ap.CheckBadAddr()
	}
	return *(*netip.AddrPort)(unsafe.Pointer(&ap))
}

func (ap Socksaddr) String() string {
	if debug.Enabled {
		ap.CheckBadAddr()
	}
	return net.JoinHostPort(ap.AddrString(), strconv.Itoa(int(ap.Port)))
}

func (ap Socksaddr) CheckBadAddr() {
	if ap.Addr.Is4In6() || ap.Addr.IsValid() && ap.Fqdn != "" {
		panic("bad socksaddr")
	}
}

func AddrPortFrom(ip net.IP, port uint16) netip.AddrPort {
	return netip.AddrPortFrom(AddrFromIP(ip), port)
}

func SocksaddrFrom(addr netip.Addr, port uint16) Socksaddr {
	return SocksaddrFromNetIP(netip.AddrPortFrom(addr, port))
}

func SocksaddrFromNetIP(ap netip.AddrPort) Socksaddr {
	return Socksaddr{
		Addr: ap.Addr(),
		Port: ap.Port(),
	}
}

func SocksaddrFromNet(ap net.Addr) Socksaddr {
	if ap == nil {
		return Socksaddr{}
	}
	if socksAddr, ok := ap.(Socksaddr); ok {
		return socksAddr
	}
	addr := SocksaddrFromNetIP(AddrPortFromNet(ap))
	if addr.IsValid() {
		return addr
	}
	return ParseSocksaddr(ap.String())
}

func AddrFromNet(netAddr net.Addr) netip.Addr {
	if addr := AddrPortFromNet(netAddr); addr.Addr().IsValid() {
		return addr.Addr()
	}
	switch addr := netAddr.(type) {
	case Socksaddr:
		return addr.Addr
	case *net.IPAddr:
		return AddrFromIP(addr.IP)
	case *net.IPNet:
		return AddrFromIP(addr.IP)
	default:
		return netip.Addr{}
	}
}

func PrefixFromNet(netAddr net.Addr) netip.Prefix {
	switch addr := netAddr.(type) {
	case *net.IPNet:
		bits, _ := addr.Mask.Size()
		return netip.PrefixFrom(AddrFromIP(addr.IP).Unmap(), bits)
	default:
		return netip.Prefix{}
	}
}

func AddrPortFromNet(netAddr net.Addr) netip.AddrPort {
	var ip net.IP
	var port uint16
	switch addr := netAddr.(type) {
	case Socksaddr:
		return addr.AddrPort()
	case *net.TCPAddr:
		ip = addr.IP
		port = uint16(addr.Port)
	case *net.UDPAddr:
		ip = addr.IP
		port = uint16(addr.Port)
	case *net.IPAddr:
		ip = addr.IP
	}
	return netip.AddrPortFrom(AddrFromIP(ip), port)
}

func AddrFromIP(ip net.IP) netip.Addr {
	addr, _ := netip.AddrFromSlice(ip)
	return addr
}

func ParseAddr(address string) netip.Addr {
	addr, _ := netip.ParseAddr(unwrapIPv6Address(address))
	return addr
}

func ParseSocksaddr(address string) Socksaddr {
	host, port, err := net.SplitHostPort(address)
	if err != nil {
		return ParseSocksaddrHostPort(address, 0)
	}
	return ParseSocksaddrHostPortStr(host, port)
}

func ParseSocksaddrHostPort(host string, port uint16) Socksaddr {
	netAddr, err := netip.ParseAddr(unwrapIPv6Address(host))
	if err != nil {
		return Socksaddr{
			Fqdn: host,
			Port: port,
		}
	} else {
		return Socksaddr{
			Addr: netAddr,
			Port: port,
		}
	}
}

func ParseSocksaddrHostPortStr(host string, portStr string) Socksaddr {
	port, _ := strconv.Atoi(portStr)
	netAddr, err := netip.ParseAddr(unwrapIPv6Address(host))
	if err != nil {
		return Socksaddr{
			Fqdn: host,
			Port: uint16(port),
		}
	} else {
		return Socksaddr{
			Addr: netAddr,
			Port: uint16(port),
		}
	}
}

func unwrapIPv6Address(address string) string {
	if len(address) > 2 && address[0] == '[' && address[len(address)-1] == ']' {
		return address[1 : len(address)-1]
	}
	return address
}
