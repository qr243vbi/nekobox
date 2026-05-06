//go:build js || wasip1

package netx

// Copied and modified from src/net/ipsock_posix.go

func probe() IPStackCapabilities {
	// Both ipv4 and ipv6 are faked; see src/net/net_fake.go.
	return IPStackCapabilities{
		IPv4Enabled:           true,
		IPv6Enabled:           true,
		IPv4MappedIPv6Enabled: true,
	}
}
