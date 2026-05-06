package ipnlocal

import (
	"sync/atomic"

	"github.com/sagernet/tailscale/wgengine"
	"github.com/sagernet/tailscale/wgengine/filter"
)

func (b *LocalBackend) ExportFilter() *atomic.Pointer[filter.Filter] {
	return &b.currentNode().filterAtomic
}

func (b *LocalBackend) ExportEngine() wgengine.Engine {
	return b.e
}
