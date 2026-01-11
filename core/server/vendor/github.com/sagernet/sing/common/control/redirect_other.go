//go:build !linux && !darwin

package control

import (
	"net"
	"net/netip"
	"os"
)

func GetOriginalDestination(conn net.Conn) (netip.AddrPort, error) {
	return netip.AddrPort{}, os.ErrInvalid
}
