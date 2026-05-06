package tun

import (
	"context"
	"net/netip"
	"sync"
	"time"

	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type TCPNat struct {
	timeout    time.Duration
	portIndex  uint16
	portAccess sync.RWMutex
	addrAccess sync.RWMutex
	addrMap    map[netip.AddrPort]uint16
	portMap    map[uint16]*TCPSession
}

type TCPSession struct {
	sync.Mutex
	Source      netip.AddrPort
	Destination netip.AddrPort
	LastActive  time.Time
}

func NewNat(ctx context.Context, timeout time.Duration) *TCPNat {
	natMap := &TCPNat{
		timeout:   timeout,
		portIndex: 10000,
		addrMap:   make(map[netip.AddrPort]uint16),
		portMap:   make(map[uint16]*TCPSession),
	}
	go natMap.loopCheckTimeout(ctx)
	return natMap
}

func (n *TCPNat) loopCheckTimeout(ctx context.Context) {
	ticker := time.NewTicker(n.timeout)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			n.checkTimeout()
		case <-ctx.Done():
			return
		}
	}
}

func (n *TCPNat) checkTimeout() {
	now := time.Now()
	n.portAccess.Lock()
	defer n.portAccess.Unlock()
	n.addrAccess.Lock()
	defer n.addrAccess.Unlock()
	for natPort, session := range n.portMap {
		session.Lock()
		if now.Sub(session.LastActive) > n.timeout {
			delete(n.addrMap, session.Source)
			delete(n.portMap, natPort)
		}
		session.Unlock()
	}
}

func (n *TCPNat) LookupBack(port uint16) *TCPSession {
	n.portAccess.RLock()
	session := n.portMap[port]
	n.portAccess.RUnlock()
	if session != nil {
		session.Lock()
		if time.Since(session.LastActive) > time.Second {
			session.LastActive = time.Now()
		}
		session.Unlock()
	}
	return session
}

func (n *TCPNat) Lookup(source netip.AddrPort, destination netip.AddrPort, handler Handler) (uint16, error) {
	n.addrAccess.RLock()
	port, loaded := n.addrMap[source]
	n.addrAccess.RUnlock()
	if loaded {
		return port, nil
	}
	_, pErr := handler.PrepareConnection(N.NetworkTCP, M.SocksaddrFromNetIP(source), M.SocksaddrFromNetIP(destination), nil, 0)
	if pErr != nil {
		return 0, pErr
	}
	n.addrAccess.Lock()
	nextPort := n.portIndex
	if nextPort == 0 {
		nextPort = 10000
		n.portIndex = 10001
	} else {
		n.portIndex++
	}
	n.addrMap[source] = nextPort
	n.addrAccess.Unlock()
	n.portAccess.Lock()
	n.portMap[nextPort] = &TCPSession{
		Source:      source,
		Destination: destination,
		LastActive:  time.Now(),
	}
	n.portAccess.Unlock()
	return nextPort, nil
}
