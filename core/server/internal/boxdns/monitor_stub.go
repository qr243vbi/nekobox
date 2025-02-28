//go:build !windows

package boxdns

import (
	tun "github.com/sagernet/sing-tun"
)

var DefaultIfcMonitor tun.DefaultInterfaceMonitor

func monitorForUnderlyingDNS() {
	return
}
