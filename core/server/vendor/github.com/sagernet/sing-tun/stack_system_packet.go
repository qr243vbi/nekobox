package tun

import (
	"net/netip"
	"syscall"

	"github.com/sagernet/sing-tun/internal/gtcpip/header"
)

func PacketIPVersion(packet []byte) int {
	return header.IPVersion(packet)
}

func PacketFillHeader(packet []byte, ipVersion int) {
	if PacketOffset > 0 {
		switch ipVersion {
		case header.IPv4Version:
			packet[3] = syscall.AF_INET
		case header.IPv6Version:
			packet[3] = syscall.AF_INET6
		}
	}
}

func PacketDestination(packet []byte) netip.Addr {
	switch ipVersion := header.IPVersion(packet); ipVersion {
	case header.IPv4Version:
		return header.IPv4(packet).DestinationAddr()
	case header.IPv6Version:
		return header.IPv6(packet).DestinationAddr()
	default:
		return netip.Addr{}
	}
}
