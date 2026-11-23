package boxbox

import (
	"context"
	"testing"
	"time"

	"github.com/miekg/dns"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/dns/transport"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing/service"
)

type waitingDNSRouter struct {
	adapter.DNSRouter
	entered    chan struct{}
	packetRead chan struct{}
	ctx        context.Context
	message    *dns.Msg
	options    adapter.DNSQueryOptions
	pool       *transport.ConnPool[bool]
}

func (r *waitingDNSRouter) ExchangeAsync(ctx context.Context, message *dns.Msg, options adapter.DNSQueryOptions, callback func(*dns.Msg, error)) {
	r.ctx, r.message, r.options = ctx, message, options
	// Model a transport waiting for a handshake packet on the same TUN reader.
	// Use the actual pinned transport pool, including its synchronous wait.
	conn, _, _, err := r.pool.AcquireShared(ctx, func(ctx context.Context) (bool, error) {
		close(r.entered)
		select {
		case <-r.packetRead:
			return true, nil
		case <-ctx.Done():
			return false, ctx.Err()
		}
	})
	if err == nil {
		r.pool.Release(conn, true)
	}
	callback(message, err)
}

func TestDNSExchangeDoesNotBlockTUNPacketReader(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	underlying := &waitingDNSRouter{entered: make(chan struct{}), packetRead: make(chan struct{})}
	underlying.pool = transport.NewConnPool(transport.ConnPoolOptions[bool]{
		Mode:    transport.ConnPoolSingle,
		IsAlive: func(conn bool) bool { return conn },
		Close:   func(bool, error) {},
	})
	router := &asyncDNSRouter{DNSRouter: underlying}
	message := new(dns.Msg)
	message.SetQuestion("example.com.", dns.TypeA)
	ttl := uint32(60)
	options := adapter.DNSQueryOptions{DisableCache: true, RewriteTTL: &ttl, Timeout: 5 * time.Second}
	returned := make(chan struct{})
	completed := make(chan struct{})
	go func() {
		router.ExchangeAsync(ctx, message, options, func(response *dns.Msg, err error) {
			if err != nil || response != message {
				t.Errorf("callback changed: response=%p err=%v", response, err)
			}
			close(completed)
		})
		// The packet reader can deliver the handshake only after dispatch returns.
		close(returned)
		close(underlying.packetRead)
	}()
	select {
	case <-returned:
	case <-time.After(time.Second):
		t.Fatal("DNS connection setup blocked the TUN packet reader")
	}
	select {
	case <-completed:
	case <-time.After(time.Second):
		t.Fatal("DNS callback did not finish after reading the handshake packet")
	}
	if underlying.ctx != ctx || underlying.message != message || underlying.options != options {
		t.Fatal("DNS request context, message or query options changed")
	}
}

func TestBoxRegistersAsyncDNSRouter(t *testing.T) {
	instance, err := New(Options{Context: include.Context(context.Background())})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = instance.Close() })
	router, ok := service.FromContext[adapter.DNSRouter](instance.Context()).(*asyncDNSRouter)
	if !ok || router.DNSRouter != instance.dnsRouter {
		t.Fatal("route construction must use the asynchronous DNS adapter")
	}
}
