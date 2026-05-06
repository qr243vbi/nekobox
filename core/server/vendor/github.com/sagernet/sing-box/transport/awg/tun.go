package awg

import (
	awgTun "github.com/amnezia-vpn/amneziawg-go/tun"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing/common/network"
)

type tunAdapter interface {
	network.Dialer
	awgTun.Device
	adapter.SimpleLifecycle
}
