package boxbox_test

import (
	"context"
	"testing"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/service"

	"nekobox_core/internal/boxbox"
)

// ReferenceManager.update requires ProviderManager even with no providers configured.
// Exercise construction only; application connectivity is tested manually.
func TestBoxRegistersProviderManager(t *testing.T) {
	ctx := boxbox.Context(context.Background(), include.InboundRegistry(), include.OutboundRegistry(), include.EndpointRegistry(), include.ProviderRegistry(), include.DNSTransportRegistry(), include.ServiceRegistry(), include.CertificateProviderRegistry())
	instance, err := boxbox.New(boxbox.Options{Context: ctx})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = instance.Close() })
	manager := service.FromContext[adapter.ProviderManager](instance.Context())
	if manager == nil {
		t.Fatal("ProviderManager is missing from the box service registry")
	}
	if got := len(manager.Providers()); got != 0 {
		t.Fatalf("empty configuration has %d providers", got)
	}
}

func TestBoxRegistersTrafficCounters(t *testing.T) {
	ctx := include.Context(context.Background())
	instance, err := boxbox.New(boxbox.Options{
		Context: ctx,
		Options: option.Options{
			Experimental: &option.ExperimentalOptions{
				ClashAPI: &option.ClashAPIOptions{ExternalController: "127.0.0.1:0"},
			},
		},
	})
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = instance.Close() })
	if service.PtrFromContext[boxbox.TrafficCounters](instance.Context()) == nil {
		t.Fatal("QueryStats traffic counters are missing from the box service registry")
	}
}
