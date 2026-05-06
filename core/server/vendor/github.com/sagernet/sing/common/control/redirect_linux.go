package control

import (
	"encoding/binary"
	"net"
	"net/netip"
	"os"
	"syscall"

	"github.com/sagernet/sing/common"
	M "github.com/sagernet/sing/common/metadata"

	"golang.org/x/sys/unix"
)

func GetOriginalDestination(conn net.Conn) (netip.AddrPort, error) {
	syscallConn, loaded := common.Cast[syscall.Conn](conn)
	if !loaded {
		return netip.AddrPort{}, os.ErrInvalid
	}
	return Conn0[netip.AddrPort](syscallConn, func(fd uintptr) (netip.AddrPort, error) {
		if M.SocksaddrFromNet(conn.RemoteAddr()).Unwrap().IsIPv4() {
			raw, err := unix.GetsockoptIPv6Mreq(int(fd), unix.IPPROTO_IP, unix.SO_ORIGINAL_DST)
			if err != nil {
				return netip.AddrPort{}, err
			}
			return netip.AddrPortFrom(M.AddrFromIP(raw.Multiaddr[4:8]), uint16(raw.Multiaddr[2])<<8+uint16(raw.Multiaddr[3])), nil
		} else {
			raw, err := unix.GetsockoptIPv6MTUInfo(int(fd), unix.IPPROTO_IPV6, unix.SO_ORIGINAL_DST)
			if err != nil {
				return netip.AddrPort{}, err
			}
			var port [2]byte
			binary.BigEndian.PutUint16(port[:], raw.Addr.Port)
			return netip.AddrPortFrom(M.AddrFromIP(raw.Addr.Addr[:]), binary.LittleEndian.Uint16(port[:])), nil
		}
	})
}
