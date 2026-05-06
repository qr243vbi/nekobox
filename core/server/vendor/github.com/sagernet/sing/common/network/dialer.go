package network

import (
	"context"
	"net"
	"net/netip"

	M "github.com/sagernet/sing/common/metadata"
)

type Dialer interface {
	DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error)
	ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error)
}

type ParallelDialer interface {
	Dialer
	DialParallel(ctx context.Context, network string, destination M.Socksaddr, destinationAddresses []netip.Addr) (net.Conn, error)
}

var SystemDialer ParallelDialer = &DefaultDialer{}

type DefaultDialer struct {
	net.Dialer
	net.ListenConfig
}

func (d *DefaultDialer) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	return d.Dialer.DialContext(ctx, network, destination.String())
}

func (d *DefaultDialer) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	return d.ListenConfig.ListenPacket(ctx, "udp", "")
}

func (d *DefaultDialer) DialParallel(ctx context.Context, network string, destination M.Socksaddr, destinationAddresses []netip.Addr) (net.Conn, error) {
	return DialParallel(ctx, d, network, destination, destinationAddresses, false, 0)
}
