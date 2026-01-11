package tun

import (
	"strconv"

	"github.com/sagernet/sing-tun/internal/gtcpip"
	"github.com/sagernet/sing-tun/internal/gtcpip/header"
	F "github.com/sagernet/sing/common/format"
	N "github.com/sagernet/sing/common/network"
)

func NetworkName(network uint8) string {
	switch tcpip.TransportProtocolNumber(network) {
	case header.TCPProtocolNumber:
		return N.NetworkTCP
	case header.UDPProtocolNumber:
		return N.NetworkUDP
	case header.ICMPv4ProtocolNumber:
		return N.NetworkICMPv4
	case header.ICMPv6ProtocolNumber:
		return N.NetworkICMPv6
	}
	return F.ToString(network)
}

func NetworkFromName(name string) uint8 {
	switch name {
	case N.NetworkTCP:
		return uint8(header.TCPProtocolNumber)
	case N.NetworkUDP:
		return uint8(header.UDPProtocolNumber)
	case N.NetworkICMPv4:
		return uint8(header.ICMPv4ProtocolNumber)
	case N.NetworkICMPv6:
		return uint8(header.ICMPv6ProtocolNumber)
	}
	parseNetwork, err := strconv.ParseUint(name, 10, 8)
	if err != nil {
		return 0
	}
	return uint8(parseNetwork)
}
