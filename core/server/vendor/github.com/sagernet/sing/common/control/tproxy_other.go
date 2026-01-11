//go:build !linux

package control

import (
	"net/netip"
	"os"
)

func TProxy(fd uintptr, isIPv6 bool) error {
	return os.ErrInvalid
}

func TProxyWriteBack() Func {
	return nil
}

func GetOriginalDestinationFromOOB(oob []byte) (netip.AddrPort, error) {
	return netip.AddrPort{}, os.ErrInvalid
}
