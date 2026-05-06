//go:build with_gvisor

package tun

import (
	"context"
	"net/netip"
	"runtime"
	"time"

	"github.com/sagernet/gvisor/pkg/tcpip"
	"github.com/sagernet/gvisor/pkg/tcpip/adapters/gonet"
	"github.com/sagernet/gvisor/pkg/tcpip/header"
	"github.com/sagernet/gvisor/pkg/tcpip/network/ipv4"
	"github.com/sagernet/gvisor/pkg/tcpip/network/ipv6"
	"github.com/sagernet/gvisor/pkg/tcpip/stack"
	"github.com/sagernet/gvisor/pkg/tcpip/transport/icmp"
	"github.com/sagernet/gvisor/pkg/tcpip/transport/raw"
	"github.com/sagernet/gvisor/pkg/tcpip/transport/tcp"
	"github.com/sagernet/gvisor/pkg/tcpip/transport/udp"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
)

const WithGVisor = true

const DefaultNIC tcpip.NICID = 1

type GVisor struct {
	ctx                  context.Context
	tun                  GVisorTun
	inet4Address         netip.Addr
	inet6Address         netip.Addr
	inet4LoopbackAddress []netip.Addr
	inet6LoopbackAddress []netip.Addr
	udpTimeout           time.Duration
	broadcastAddr        netip.Addr
	handler              Handler
	logger               logger.Logger
	stack                *stack.Stack
	endpoint             stack.LinkEndpoint
}

type GVisorTun interface {
	Tun
	WritePacket(pkt *stack.PacketBuffer) (int, error)
	NewEndpoint() (stack.LinkEndpoint, stack.NICOptions, error)
}

func NewGVisor(
	options StackOptions,
) (Stack, error) {
	gTun, isGTun := options.Tun.(GVisorTun)
	if !isGTun {
		return nil, E.New("gVisor stack is unsupported on current platform")
	}

	var (
		inet4Address netip.Addr
		inet6Address netip.Addr
	)
	if len(options.TunOptions.Inet4Address) > 0 {
		inet4Address = options.TunOptions.Inet4Address[0].Addr()
	}
	if len(options.TunOptions.Inet6Address) > 0 {
		inet6Address = options.TunOptions.Inet6Address[0].Addr()
	}

	gStack := &GVisor{
		ctx:                  options.Context,
		tun:                  gTun,
		inet4Address:         inet4Address,
		inet6Address:         inet6Address,
		inet4LoopbackAddress: options.TunOptions.Inet4LoopbackAddress,
		inet6LoopbackAddress: options.TunOptions.Inet6LoopbackAddress,
		udpTimeout:           options.UDPTimeout,
		broadcastAddr:        BroadcastAddr(options.TunOptions.Inet4Address),
		handler:              options.Handler,
		logger:               options.Logger,
	}
	return gStack, nil
}

func (t *GVisor) Start() error {
	linkEndpoint, nicOptions, err := t.tun.NewEndpoint()
	if err != nil {
		return err
	}
	linkEndpoint = &LinkEndpointFilter{linkEndpoint, t.broadcastAddr, t.tun}
	ipStack, err := newGVisorStack(linkEndpoint, nicOptions, false, true)
	if err != nil {
		return err
	}
	ipStack.SetTransportProtocolHandler(tcp.ProtocolNumber, NewTCPForwarderWithLoopback(t.ctx, ipStack, t.handler, t.inet4LoopbackAddress, t.inet6LoopbackAddress, t.tun).HandlePacket)
	ipStack.SetTransportProtocolHandler(udp.ProtocolNumber, NewUDPForwarder(t.ctx, ipStack, t.handler, t.udpTimeout).HandlePacket)
	icmpForwarder := NewICMPForwarder(t.ctx, ipStack, t.handler, t.udpTimeout)
	icmpForwarder.SetLocalAddresses(t.inet4Address, t.inet6Address)
	ipStack.SetTransportProtocolHandler(icmp.ProtocolNumber4, icmpForwarder.HandlePacket)
	ipStack.SetTransportProtocolHandler(icmp.ProtocolNumber6, icmpForwarder.HandlePacket)
	t.stack = ipStack
	t.endpoint = linkEndpoint
	return nil
}

func (t *GVisor) Close() error {
	if t.stack == nil {
		return nil
	}
	t.endpoint.Attach(nil)
	t.stack.Close()
	for _, endpoint := range t.stack.CleanupEndpoints() {
		endpoint.Abort()
	}
	return nil
}

func AddressFromAddr(destination netip.Addr) tcpip.Address {
	if destination.Is6() {
		return tcpip.AddrFrom16(destination.As16())
	} else {
		return tcpip.AddrFrom4(destination.As4())
	}
}

func AddrFromAddress(address tcpip.Address) netip.Addr {
	if address.Len() == 16 {
		return netip.AddrFrom16(address.As16())
	} else {
		return netip.AddrFrom4(address.As4())
	}
}

func NewGVisorStack(ep stack.LinkEndpoint) (*stack.Stack, error) {
	return NewGVisorStackWithOptions(ep, stack.NICOptions{}, false)
}

func NewGVisorStackWithOptions(ep stack.LinkEndpoint, opts stack.NICOptions, allowRawEndpoint bool) (*stack.Stack, error) {
	return newGVisorStack(ep, opts, allowRawEndpoint, false)
}

func newGVisorStack(ep stack.LinkEndpoint, opts stack.NICOptions, allowRawEndpoint bool, isLocalStack bool) (*stack.Stack, error) {
	stackOptions := stack.Options{
		NetworkProtocols: []stack.NetworkProtocolFactory{
			ipv4.NewProtocol,
			ipv6.NewProtocol,
		},
		TransportProtocols: []stack.TransportProtocolFactory{
			tcp.NewProtocol,
			udp.NewProtocol,
			icmp.NewProtocol4,
			icmp.NewProtocol6,
		},
	}
	if allowRawEndpoint {
		stackOptions.RawFactory = new(raw.EndpointFactory)
	}
	ipStack := stack.New(stackOptions)
	err := ipStack.CreateNICWithOptions(DefaultNIC, ep, opts)
	if err != nil {
		return nil, gonet.TranslateNetstackError(err)
	}
	ipStack.SetRouteTable([]tcpip.Route{
		{Destination: header.IPv4EmptySubnet, NIC: DefaultNIC},
		{Destination: header.IPv6EmptySubnet, NIC: DefaultNIC},
	})
	err = ipStack.SetSpoofing(DefaultNIC, true)
	if err != nil {
		return nil, gonet.TranslateNetstackError(err)
	}
	err = ipStack.SetPromiscuousMode(DefaultNIC, true)
	if err != nil {
		return nil, gonet.TranslateNetstackError(err)
	}
	sOpt := tcpip.TCPSACKEnabled(true)
	ipStack.SetTransportProtocolOption(tcp.ProtocolNumber, &sOpt)
	mOpt := tcpip.TCPModerateReceiveBufferOption(true)
	ipStack.SetTransportProtocolOption(tcp.ProtocolNumber, &mOpt)
	if runtime.GOOS == "windows" {
		tcpRecoveryOpt := tcpip.TCPRecovery(0)
		err = ipStack.SetTransportProtocolOption(tcp.ProtocolNumber, &tcpRecoveryOpt)
	}
	if isLocalStack || runtime.GOOS == "ios" {
		const iOSBufferDefault = 1 << 15
		const iOSBufferMax = 1 << 17
		tcpRXBufOpt := tcpip.TCPReceiveBufferSizeRangeOption{
			Min:     tcpRXBufMinSize,
			Default: iOSBufferDefault,
			Max:     iOSBufferMax,
		}
		err = ipStack.SetTransportProtocolOption(tcp.ProtocolNumber, &tcpRXBufOpt)
		if err != nil {
			return nil, gonet.TranslateNetstackError(err)
		}
		tcpTXBufOpt := tcpip.TCPSendBufferSizeRangeOption{
			Min:     tcpTXBufMinSize,
			Default: iOSBufferDefault,
			Max:     iOSBufferMax,
		}
		err = ipStack.SetTransportProtocolOption(tcp.ProtocolNumber, &tcpTXBufOpt)
		if err != nil {
			return nil, gonet.TranslateNetstackError(err)
		}
	} else {
		tcpRXBufOpt := tcpip.TCPReceiveBufferSizeRangeOption{
			Min:     tcpRXBufMinSize,
			Default: tcpRXBufDefSize,
			Max:     tcpRXBufMaxSize,
		}
		err = ipStack.SetTransportProtocolOption(tcp.ProtocolNumber, &tcpRXBufOpt)
		if err != nil {
			return nil, gonet.TranslateNetstackError(err)
		}
		tcpTXBufOpt := tcpip.TCPSendBufferSizeRangeOption{
			Min:     tcpTXBufMinSize,
			Default: tcpTXBufDefSize,
			Max:     tcpTXBufMaxSize,
		}
		err = ipStack.SetTransportProtocolOption(tcp.ProtocolNumber, &tcpTXBufOpt)
		if err != nil {
			return nil, gonet.TranslateNetstackError(err)
		}
	}
	return ipStack, nil
}
