//go:build with_gvisor

package tun

import (
	"context"
	"errors"
	"net/netip"

	"github.com/sagernet/gvisor/pkg/tcpip"
	"github.com/sagernet/gvisor/pkg/tcpip/header"
	"github.com/sagernet/gvisor/pkg/tcpip/stack"
	"github.com/sagernet/gvisor/pkg/tcpip/transport/tcp"
	"github.com/sagernet/sing-tun/internal/gtcpip/checksum"
	"github.com/sagernet/sing/common"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type TCPForwarder struct {
	ctx                  context.Context
	stack                *stack.Stack
	handler              Handler
	inet4LoopbackAddress []tcpip.Address
	inet6LoopbackAddress []tcpip.Address
	tun                  GVisorTun
	forwarder            *tcp.Forwarder
}

func NewTCPForwarder(ctx context.Context, stack *stack.Stack, handler Handler) *TCPForwarder {
	return NewTCPForwarderWithLoopback(ctx, stack, handler, nil, nil, nil)
}

func NewTCPForwarderWithLoopback(ctx context.Context, stack *stack.Stack, handler Handler, inet4LoopbackAddress []netip.Addr, inet6LoopbackAddress []netip.Addr, tun GVisorTun) *TCPForwarder {
	forwarder := &TCPForwarder{
		ctx:                  ctx,
		stack:                stack,
		handler:              handler,
		inet4LoopbackAddress: common.Map(inet4LoopbackAddress, AddressFromAddr),
		inet6LoopbackAddress: common.Map(inet6LoopbackAddress, AddressFromAddr),
		tun:                  tun,
	}
	forwarder.forwarder = tcp.NewForwarder(stack, 0, 1024, forwarder.Forward)
	return forwarder
}

func (f *TCPForwarder) HandlePacket(id stack.TransportEndpointID, pkt *stack.PacketBuffer) bool {
	for _, inet4LoopbackAddress := range f.inet4LoopbackAddress {
		if id.LocalAddress == inet4LoopbackAddress {
			ipHdr := pkt.Network().(header.IPv4)
			ipHdr.SetDestinationAddressWithChecksumUpdate(ipHdr.SourceAddress())
			ipHdr.SetSourceAddressWithChecksumUpdate(inet4LoopbackAddress)
			tcpHdr := header.TCP(pkt.TransportHeader().Slice())
			tcpHdr.SetChecksum(0)
			tcpHdr.SetChecksum(^checksum.Combine(pkt.Data().Checksum(), tcpHdr.CalculateChecksum(
				header.PseudoHeaderChecksum(header.TCPProtocolNumber, ipHdr.SourceAddress(), ipHdr.DestinationAddress(), ipHdr.PayloadLength()),
			)))
			f.tun.WritePacket(pkt)
			return true
		}
	}
	for _, inet6LoopbackAddress := range f.inet6LoopbackAddress {
		if id.LocalAddress == inet6LoopbackAddress {
			ipHdr := pkt.Network().(header.IPv6)
			ipHdr.SetDestinationAddress(ipHdr.SourceAddress())
			ipHdr.SetSourceAddress(inet6LoopbackAddress)
			tcpHdr := header.TCP(pkt.TransportHeader().Slice())
			tcpHdr.SetChecksum(0)
			tcpHdr.SetChecksum(^checksum.Combine(pkt.Data().Checksum(), tcpHdr.CalculateChecksum(
				header.PseudoHeaderChecksum(header.TCPProtocolNumber, ipHdr.SourceAddress(), ipHdr.DestinationAddress(), ipHdr.PayloadLength()),
			)))
			f.tun.WritePacket(pkt)
			return true
		}
	}
	return f.forwarder.HandlePacket(id, pkt)
}

func (f *TCPForwarder) Forward(r *tcp.ForwarderRequest) {
	source := M.SocksaddrFrom(AddrFromAddress(r.ID().RemoteAddress), r.ID().RemotePort)
	destination := M.SocksaddrFrom(AddrFromAddress(r.ID().LocalAddress), r.ID().LocalPort)
	_, pErr := f.handler.PrepareConnection(N.NetworkTCP, source, destination, nil, 0)
	if pErr != nil {
		r.Complete(!errors.Is(pErr, ErrDrop))
		return
	}
	conn := &gLazyConn{
		parentCtx:  f.ctx,
		stack:      f.stack,
		request:    r,
		localAddr:  source.TCPAddr(),
		remoteAddr: destination.TCPAddr(),
	}
	go f.handler.NewConnectionEx(f.ctx, conn, source, destination, nil)
}
