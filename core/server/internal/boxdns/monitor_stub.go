//go:build !windows

package boxdns

import (
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/control"
)

var DnsManagerInstance *DnsManager

type DnsManager struct {
	Monitor tun.DefaultInterfaceMonitor
	lastIfc control.Interface
}

func (d *DnsManager) HandleSystemDNS(ifc *control.Interface, flag int) {
	return
}
