package netx

import "sync"

// Copied and modified from src/net/ipsock_posix.go

// IPStackCapabilities represents IPv4, IPv6 and IPv4-mapped IPv6
// communication capabilities which are controlled by the IPV6_V6ONLY
// socket option and kernel configuration.
type IPStackCapabilities struct {
	IPv4Enabled           bool
	IPv6Enabled           bool
	IPv4MappedIPv6Enabled bool
}

// IPStackCaps returns the IP stack capabilities of the system.
var IPStackCaps = sync.OnceValue(probe)
