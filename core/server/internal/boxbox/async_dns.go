package boxbox

import (
	"context"

	"github.com/miekg/dns"
	"github.com/sagernet/sing-box/adapter"
)

// asyncDNSRouter keeps transport connection setup off the TUN packet reader.
// The pinned sing-box transport's ExchangeAsync can synchronously wait in
// AcquireShared after a DNS-over-TLS server has passed its reuse probe. That
// connection needs packets from the same reader, so calling it inline deadlocks
// both System and gVisor. Keep all DNS rules, transports and callbacks unchanged.
type asyncDNSRouter struct {
	adapter.DNSRouter
}

func (r *asyncDNSRouter) ExchangeAsync(ctx context.Context, message *dns.Msg, options adapter.DNSQueryOptions, callback func(*dns.Msg, error)) {
	go r.DNSRouter.ExchangeAsync(ctx, message, options, callback)
}
