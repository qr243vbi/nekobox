package metadata

import "net/netip"

func NetworkFromNetAddr(network string, addr netip.Addr) string {
	if addr == netip.IPv4Unspecified() {
		return network + "4"
	}
	return network
}
