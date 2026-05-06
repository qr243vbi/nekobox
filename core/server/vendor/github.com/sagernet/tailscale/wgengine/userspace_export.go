package wgengine

import (
	"github.com/sagernet/tailscale/net/dns"
	"github.com/sagernet/tailscale/wgengine/router"
	"github.com/sagernet/tailscale/wgengine/wgcfg"
)

type ExportedUserspaceEngine interface {
	SetOnReconfigListener(listener ReconfigListener)
}

type ReconfigListener = func(cfg *wgcfg.Config, routerCfg *router.Config, dnsCfg *dns.Config)

func (e *userspaceEngine) SetOnReconfigListener(listener ReconfigListener) {
	e.mu.Lock()
	defer e.mu.Unlock()
	e.onReconfig = listener
}
