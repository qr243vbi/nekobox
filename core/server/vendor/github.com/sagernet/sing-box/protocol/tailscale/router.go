//go:build with_gvisor

package tailscale

import (
	"net/netip"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/tailscale/net/tsaddr"
	"github.com/sagernet/tailscale/wgengine/router"
)

type exitRouteFilteringRouter struct {
	router.Router
}

func (r *exitRouteFilteringRouter) Set(config *router.Config) error {
	if config != nil {
		config = config.Clone()
		config.Routes = common.Filter(config.Routes, func(prefix netip.Prefix) bool {
			return !tsaddr.IsExitRoute(prefix)
		})
	}
	return r.Router.Set(config)
}
