//go:build !darwin

package main

import (
	"context"
	"sync/atomic"
	"testing"
	"time"

	"nekobox_core/gen"
	"nekobox_core/internal"
)

// fakeSystemProxy stands in for the OS system proxy so the tests never touch it.
type fakeSystemProxy struct{ enabled atomic.Bool }

func (p *fakeSystemProxy) IsEnabled() bool { return p.enabled.Load() }
func (p *fakeSystemProxy) Enable() error   { p.enabled.Store(true); return nil }
func (p *fakeSystemProxy) Disable() error  { p.enabled.Store(false); return nil }
func (p *fakeSystemProxy) Close() error    { return nil }

// useEnabledSystemProxy mimics the GUI enabling the system proxy over RPC
// before it starts the remembered profile.
func useEnabledSystemProxy(t *testing.T) *fakeSystemProxy {
	t.Helper()
	previous := internal.SystemProxyController
	proxy := &fakeSystemProxy{}
	proxy.enabled.Store(true)
	internal.SystemProxyController = proxy
	t.Cleanup(func() { internal.SystemProxyController = previous })
	return proxy
}

func stopInstance(t *testing.T, s *server) {
	t.Helper()
	t.Cleanup(func() {
		previous := internal.SystemProxyController
		internal.SystemProxyController = nil
		_, _ = s.Stop(context.Background(), &gen.EmptyReq{})
		internal.SystemProxyController = previous
	})
}

func TestStartKeepsSystemProxy(t *testing.T) {
	proxy := useEnabledSystemProxy(t)
	s := &server{}
	stopInstance(t, s)

	out, err := s.Start(context.Background(), &gen.LoadConfigReq{
		CoreConfig: `{"log":{"disabled":true},"outbounds":[{"type":"direct","tag":"direct"}]}`,
	})
	if err != nil || out.Error != "" {
		t.Fatalf("Start = %v, %q; want success", err, out.Error)
	}
	// A reset scheduled by Start would run on another goroutine, so give it time.
	for deadline := time.Now().Add(500 * time.Millisecond); time.Now().Before(deadline); time.Sleep(10 * time.Millisecond) {
		if !proxy.IsEnabled() {
			t.Fatal("system proxy was disabled by a successful Start")
		}
	}
}

func TestFailedStartResetsSystemProxy(t *testing.T) {
	proxy := useEnabledSystemProxy(t)
	s := &server{}
	stopInstance(t, s)

	out, err := s.Start(context.Background(), &gen.LoadConfigReq{CoreConfig: `{"outbounds":[{"type":"no-such-type"}]}`})
	if err != nil || out.Error == "" {
		t.Fatalf("Start = %v, %q; want a config error", err, out.Error)
	}
	if proxy.IsEnabled() {
		t.Fatal("system proxy still points at a core that failed to start")
	}
}
