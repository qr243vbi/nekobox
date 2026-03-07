//go:build !darwin

package sys

import (
	"errors"
	tun "github.com/sagernet/sing-tun"
)

func SetSystemDNS(addr string, interfaceMonitor tun.DefaultInterfaceMonitor) error {
	return errors.New("not implemented for this OS")
}
