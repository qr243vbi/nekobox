package network

import (
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
)

var ErrUnknownNetwork = E.New("unknown network")

//goland:noinspection GoNameStartsWithPackageName
const (
	NetworkIP     = "ip"
	NetworkTCP    = "tcp"
	NetworkUDP    = "udp"
	NetworkICMPv4 = "icmpv4"
	NetworkICMPv6 = "icmpv6"
)

//goland:noinspection GoNameStartsWithPackageName
func NetworkName(network string) string {
	if strings.HasPrefix(network, "tcp") {
		return NetworkTCP
	} else if strings.HasPrefix(network, "udp") {
		return NetworkUDP
	} else if strings.HasPrefix(network, "ip") {
		return NetworkIP
	} else {
		return network
	}
}
