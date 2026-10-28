package boxbox

import (
	"bytes"
	"context"
	"io"
	"net"
	"testing"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/trafficcontrol"
)

type testOutbound struct {
	adapter.Outbound
	tag string
}

func (o *testOutbound) Tag() string  { return o.tag }
func (o *testOutbound) Type() string { return "direct" }

type testGroup struct {
	testOutbound
	now string
}

func (g *testGroup) Now() string   { return g.now }
func (g *testGroup) All() []string { return []string{g.now} }
func (g *testGroup) Type() string  { return "selector" }

type testOutboundManager struct {
	adapter.OutboundManager
	outbounds []adapter.Outbound
}

func (m *testOutboundManager) Outbounds() []adapter.Outbound { return m.outbounds }
func (m *testOutboundManager) Default() adapter.Outbound     { return m.outbounds[0] }

func (m *testOutboundManager) Outbound(tag string) (adapter.Outbound, bool) {
	for _, outbound := range m.outbounds {
		if outbound.Tag() == tag {
			return outbound, true
		}
	}
	return nil, false
}

type testEndpointManager struct {
	adapter.EndpointManager
	endpoints []adapter.Endpoint
}

func (m *testEndpointManager) Endpoints() []adapter.Endpoint { return m.endpoints }

func newTestCounters(outbounds ...adapter.Outbound) (*TrafficCounters, *testOutboundManager) {
	manager := &testOutboundManager{outbounds: outbounds}
	return NewTrafficCounters(manager, &testEndpointManager{}), manager
}

func takeAll(c *TrafficCounters) (map[string]int64, map[string]int64) {
	ups, downs := map[string]int64{}, map[string]int64{}
	c.TakeAll(ups, downs)
	return ups, downs
}

// transfer sends upload bytes from the client side through conn and download
// bytes back, then closes conn.
func transfer(t *testing.T, conn, client net.Conn, upload, download int) {
	t.Helper()
	done := make(chan error, 1)
	go func() {
		if _, err := client.Write(bytes.Repeat([]byte{1}, upload)); err != nil {
			done <- err
			return
		}
		_, err := io.CopyN(io.Discard, client, int64(download))
		done <- err
	}()
	if _, err := io.CopyN(io.Discard, conn, int64(upload)); err != nil {
		t.Fatal(err)
	}
	if _, err := conn.Write(bytes.Repeat([]byte{2}, download)); err != nil {
		t.Fatal(err)
	}
	if err := <-done; err != nil {
		t.Fatal(err)
	}
	conn.Close()
}

func TestTrafficCountersKeepTrafficOfClosedConnections(t *testing.T) {
	proxy := &testOutbound{tag: "proxy"}
	counters, outbounds := newTestCounters(proxy)
	manager := trafficcontrol.NewManager(outbounds)
	if err := manager.Start(adapter.StartStateInitialize); err != nil {
		t.Fatal(err)
	}
	defer manager.Close()

	server, client := net.Pipe()
	defer client.Close()
	// The router wraps the connection with each tracker in turn.
	conn := manager.RoutedConnection(context.Background(), server, adapter.InboundContext{}, nil, proxy)
	conn = counters.RoutedConnection(context.Background(), conn, adapter.InboundContext{}, nil, proxy)
	transfer(t, conn, client, 4321, 1234)

	// What the manager's GC cleanup runs: closed connections leave its history.
	manager.Clear()
	if closed := manager.ClosedConnections(); len(closed) != 0 {
		t.Fatalf("manager still lists %d closed connections", len(closed))
	}

	ups, downs := takeAll(counters)
	if ups["proxy"] != 4321 || downs["proxy"] != 1234 {
		t.Fatalf("proxy = %d/%d, want 4321/1234", ups["proxy"], downs["proxy"])
	}
}

func TestTrafficCountersReportIdleOutboundsAsZero(t *testing.T) {
	proxy := &testOutbound{tag: "proxy"}
	counters, _ := newTestCounters(proxy, &testOutbound{tag: "direct"})
	flow := counters.RoutedFlow(context.Background(), adapter.InboundContext{}, nil, proxy)
	flow.CountForward(5)
	flow.CountReverse(50)

	ups, downs := takeAll(counters)
	if ups["proxy"] != 5 || downs["proxy"] != 50 {
		t.Fatalf("first call: proxy = %d/%d, want 5/50", ups["proxy"], downs["proxy"])
	}
	if up, ok := ups["direct"]; !ok || up != 0 {
		t.Fatalf("first call: direct = %d, present %v; want 0, present", up, ok)
	}

	ups, downs = takeAll(counters)
	for _, tag := range []string{"proxy", "direct"} {
		if up, ok := ups[tag]; !ok || up != 0 || downs[tag] != 0 {
			t.Fatalf("idle call: %s = %d/%d, present %v; want 0/0, present", tag, up, downs[tag], ok)
		}
	}
}

func TestTrafficCountersCountEachChainTagOnce(t *testing.T) {
	wiki00 := &testOutbound{tag: "wiki00"}
	selector := &testGroup{testOutbound: testOutbound{tag: "select"}, now: "wiki00"}
	counters, _ := newTestCounters(selector, wiki00, &testOutbound{tag: "direct"})

	flow := counters.RoutedFlow(context.Background(), adapter.InboundContext{}, nil, selector)
	flow.CountForward(7)
	flow.CountReverse(9)

	ups, downs := takeAll(counters)
	if ups["select"] != 7 || ups["wiki00"] != 7 || downs["select"] != 9 || downs["wiki00"] != 9 {
		t.Fatalf("ups=%v downs=%v, want 7/9 for select and wiki00", ups, downs)
	}
	if ups["direct"] != 0 || downs["direct"] != 0 {
		t.Fatalf("direct = %d/%d, want 0/0", ups["direct"], downs["direct"])
	}
}
