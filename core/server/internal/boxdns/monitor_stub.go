//go:build !windows

package boxdns

import (
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/control"
)

var DefaultIfcMonitor tun.DefaultInterfaceMonitor

func HandleInterfaceChange(_ *control.Interface, _ int) {
	return
}
