//go:build with_gvisor

package ping

import (
	"context"
	"net/netip"
	"time"

	"github.com/sagernet/gvisor/pkg/tcpip"
	"github.com/sagernet/gvisor/pkg/tcpip/adapters/gonet"
	"github.com/sagernet/gvisor/pkg/tcpip/header"
	"github.com/sagernet/gvisor/pkg/tcpip/stack"
	"github.com/sagernet/gvisor/pkg/tcpip/transport"
	"github.com/sagernet/gvisor/pkg/waiter"
	"github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
)

var _ tun.DirectRouteDestination = (*GVisorDestination)(nil)

type GVisorDestination struct {
	ctx      context.Context
	logger   logger.ContextLogger
	endpoint tcpip.Endpoint
	conn     *gonet.TCPConn
	rewriter *SourceRewriter
	timeout  time.Duration
}

func ConnectGVisor(
	ctx context.Context, logger logger.ContextLogger,
	sourceAddress, destinationAddress netip.Addr,
	routeContext tun.DirectRouteContext,
	stack *stack.Stack,
	bindAddress4, bindAddress6 netip.Addr,
	timeout time.Duration,
) (*GVisorDestination, error) {
	var (
		bindAddress tcpip.Address
		wq          waiter.Queue
		endpoint    tcpip.Endpoint
		gErr        tcpip.Error
	)
	if !destinationAddress.Is6() {
		if !bindAddress4.IsValid() {
			return nil, E.New("missing IPv4 interface address")
		}
		bindAddress = tun.AddressFromAddr(bindAddress4)
		endpoint, gErr = stack.NewRawEndpoint(header.ICMPv4ProtocolNumber, header.IPv4ProtocolNumber, &wq, true)
	} else {
		if !bindAddress6.IsValid() {
			return nil, E.New("missing IPv6 interface address")
		}
		bindAddress = tun.AddressFromAddr(bindAddress6)
		endpoint, gErr = stack.NewRawEndpoint(header.ICMPv6ProtocolNumber, header.IPv6ProtocolNumber, &wq, true)
	}
	if gErr != nil {
		return nil, gonet.TranslateNetstackError(gErr)
	}
	gErr = endpoint.Bind(tcpip.FullAddress{
		NIC:  1,
		Addr: bindAddress,
	})
	if gErr != nil {
		return nil, gonet.TranslateNetstackError(gErr)
	}
	gErr = endpoint.Connect(tcpip.FullAddress{
		NIC:  1,
		Addr: tun.AddressFromAddr(destinationAddress),
	})
	if gErr != nil {
		return nil, gonet.TranslateNetstackError(gErr)
	}
	endpoint.SocketOptions().SetHeaderIncluded(true)
	rewriter := NewSourceRewriter(ctx, logger, bindAddress4, bindAddress6)
	rewriter.CreateSession(tun.DirectRouteSession{Source: sourceAddress, Destination: destinationAddress}, routeContext)
	destination := &GVisorDestination{
		ctx:      ctx,
		logger:   logger,
		endpoint: endpoint,
		conn:     gonet.NewTCPConn(&wq, endpoint),
		rewriter: rewriter,
		timeout:  timeout,
	}
	go destination.loopRead()
	return destination, nil
}

func (d *GVisorDestination) loopRead() {
	defer d.endpoint.Close()
	for {
		buffer := buf.NewPacket()
		err := d.conn.SetReadDeadline(time.Now().Add(d.timeout))
		if err != nil {
			d.logger.ErrorContext(d.ctx, E.Cause(err, "set read deadline for ICMP conn"))
		}
		n, err := d.conn.Read(buffer.FreeBytes())
		if err != nil {
			buffer.Release()
			if !E.IsClosed(err) {
				d.logger.ErrorContext(d.ctx, E.Cause(err, "receive ICMP echo reply"))
			}
			return
		}
		buffer.Truncate(n)
		_, err = d.rewriter.WriteBack(buffer.Bytes())
		if err != nil {
			d.logger.ErrorContext(d.ctx, E.Cause(err, "write ICMP echo reply"))
		}
		buffer.Release()
	}
}

func (d *GVisorDestination) WritePacket(packet *buf.Buffer) error {
	d.rewriter.RewritePacket(packet.Bytes())
	return common.Error(d.conn.Write(packet.Bytes()))
}

func (d *GVisorDestination) Close() error {
	return d.conn.Close()
}

func (d *GVisorDestination) IsClosed() bool {
	return transport.DatagramEndpointState(d.endpoint.State()) == transport.DatagramEndpointStateClosed
}
