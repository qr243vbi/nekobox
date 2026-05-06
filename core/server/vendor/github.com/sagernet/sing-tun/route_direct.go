package tun

import (
	"net/netip"
	"time"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/contrab/freelru"
	"github.com/sagernet/sing/contrab/maphash"
)

type DirectRouteDestination interface {
	WritePacket(packet *buf.Buffer) error
	Close() error
	IsClosed() bool
}

type DirectRouteSession struct {
	// IPVersion uint8
	// Network     uint8
	Source      netip.Addr
	Destination netip.Addr
}

type DirectRouteMapping struct {
	mapping freelru.Cache[DirectRouteSession, DirectRouteDestination]
	timeout time.Duration
}

func NewDirectRouteMapping(timeout time.Duration) *DirectRouteMapping {
	mapping := common.Must1(freelru.NewSharded[DirectRouteSession, DirectRouteDestination](1024, maphash.NewHasher[DirectRouteSession]().Hash32))
	mapping.SetHealthCheck(func(session DirectRouteSession, action DirectRouteDestination) bool {
		if action != nil {
			return !action.IsClosed()
		}
		return true
	})
	mapping.SetOnEvict(func(session DirectRouteSession, action DirectRouteDestination) {
		if action != nil {
			action.Close()
		}
	})
	mapping.SetLifetime(timeout)
	return &DirectRouteMapping{mapping, timeout}
}

func (m *DirectRouteMapping) Lookup(session DirectRouteSession, constructor func(timeout time.Duration) (DirectRouteDestination, error)) (DirectRouteDestination, error) {
	var (
		created DirectRouteDestination
		err     error
	)
	action, _, ok := m.mapping.GetAndRefreshOrAdd(session, func() (DirectRouteDestination, bool) {
		created, err = constructor(m.timeout)
		return created, err == nil
	})
	if !ok {
		return nil, err
	}
	return action, nil
}
