package boxbox

import (
	"context"
	"net"
	"sync"
	"sync/atomic"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/bufio"
	N "github.com/sagernet/sing/common/network"
)

var _ adapter.ConnectionTracker = (*TrafficCounters)(nil)

// TrafficCounters counts the bytes of every routed connection per outbound tag
// for QueryStats. Per-tag totals rebuilt from trafficcontrol.Manager lose
// traffic: the manager keeps only a bounded history of closed connections, and
// clears it on GC. These counters are fed by the connections themselves and
// never expire.
type TrafficCounters struct {
	outbound adapter.OutboundManager
	endpoint adapter.EndpointManager
	access   sync.Mutex
	counters map[string]*tagCounter
}

type tagCounter struct {
	upload   atomic.Int64
	download atomic.Int64
}

func NewTrafficCounters(outbound adapter.OutboundManager, endpoint adapter.EndpointManager) *TrafficCounters {
	return &TrafficCounters{
		outbound: outbound,
		endpoint: endpoint,
		counters: make(map[string]*tagCounter),
	}
}

// TakeAll stores in ups and downs the bytes of every outbound and endpoint since
// the previous call. Idle ones are reported as zero: the GUI advances an item's
// rate clock only when its tag is present.
func (c *TrafficCounters) TakeAll(ups, downs map[string]int64) {
	for _, outbound := range c.outbound.Outbounds() {
		ups[outbound.Tag()], downs[outbound.Tag()] = 0, 0
	}
	for _, endpoint := range c.endpoint.Endpoints() {
		ups[endpoint.Tag()], downs[endpoint.Tag()] = 0, 0
	}
	c.access.Lock()
	defer c.access.Unlock()
	for tag, counter := range c.counters {
		ups[tag] = counter.upload.Swap(0)
		downs[tag] = counter.download.Swap(0)
	}
}

// countersFor returns the counters of the matched outbound and of the selected
// member of each group down to the outbound that carries the connection, each
// tag once. It walks the chain the same way trafficcontrol.Manager does.
func (c *TrafficCounters) countersFor(matchOutbound adapter.Outbound) (uploads, downloads []*atomic.Int64) {
	var next string
	if matchOutbound != nil {
		next = matchOutbound.Tag()
	} else if defaultOutbound := c.outbound.Default(); defaultOutbound != nil {
		next = defaultOutbound.Tag()
	}
	var tags []string
	for {
		detour, loaded := c.outbound.Outbound(next)
		if !loaded || containsTag(tags, next) {
			break
		}
		tags = append(tags, next)
		group, isGroup := detour.(adapter.OutboundGroup)
		if !isGroup {
			break
		}
		next = group.Now()
	}
	c.access.Lock()
	defer c.access.Unlock()
	for _, tag := range tags {
		counter := c.counters[tag]
		if counter == nil {
			counter = new(tagCounter)
			c.counters[tag] = counter
		}
		uploads = append(uploads, &counter.upload)
		downloads = append(downloads, &counter.download)
	}
	return
}

func containsTag(tags []string, tag string) bool {
	for _, item := range tags {
		if item == tag {
			return true
		}
	}
	return false
}

func (c *TrafficCounters) RoutedConnection(ctx context.Context, conn net.Conn, metadata adapter.InboundContext, matchedRule adapter.Rule, matchOutbound adapter.Outbound) net.Conn {
	uploads, downloads := c.countersFor(matchOutbound)
	if len(uploads) == 0 {
		return conn
	}
	return bufio.NewInt64CounterConn(conn, uploads, downloads)
}

func (c *TrafficCounters) RoutedPacketConnection(ctx context.Context, conn N.PacketConn, metadata adapter.InboundContext, matchedRule adapter.Rule, matchOutbound adapter.Outbound) N.PacketConn {
	uploads, downloads := c.countersFor(matchOutbound)
	if len(uploads) == 0 {
		return conn
	}
	return bufio.NewInt64CounterPacketConn(conn, uploads, nil, downloads, nil)
}

func (c *TrafficCounters) RoutedFlow(ctx context.Context, metadata adapter.InboundContext, matchedRule adapter.Rule, matchOutbound adapter.Outbound) tun.FlowTracker {
	uploads, downloads := c.countersFor(matchOutbound)
	if len(uploads) == 0 {
		return nil
	}
	return &flowCounter{uploads: uploads, downloads: downloads}
}

type flowCounter struct {
	uploads   []*atomic.Int64
	downloads []*atomic.Int64
}

func (f *flowCounter) AttachFlow(tun.FlowHandle) {}

func (f *flowCounter) CountForward(n int) {
	for _, counter := range f.uploads {
		counter.Add(int64(n))
	}
}

func (f *flowCounter) CountReverse(n int) {
	for _, counter := range f.downloads {
		counter.Add(int64(n))
	}
}

func (f *flowCounter) FlowEstablished() {}

func (f *flowCounter) CloseFlow(tun.FlowCloseReason) {}
