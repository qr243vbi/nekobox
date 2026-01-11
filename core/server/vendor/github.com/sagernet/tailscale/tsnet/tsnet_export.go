package tsnet

import (
	"github.com/sagernet/tailscale/ipn/ipnlocal"
	"github.com/sagernet/tailscale/wgengine/netstack"
)

func (s *Server) ExportNetstack() *netstack.Impl {
	return s.netstack
}

func (s *Server) ExportLocalBackend() *ipnlocal.LocalBackend {
	return s.lb
}
