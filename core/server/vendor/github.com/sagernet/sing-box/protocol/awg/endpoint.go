package awg

import (
	"context"
	"encoding/base64"
	"encoding/hex"
	"net"
	"net/netip"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/adapter/endpoint"
	"github.com/sagernet/sing-box/common/dialer"
	"github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing-box/transport/awg"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/format"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/service"

	"go4.org/netipx"
)

func RegisterEndpoint(registry *endpoint.Registry) {
	endpoint.Register(registry, constant.TypeAwg, NewEndpoint)
}

type Endpoint struct {
	*awg.Device
	endpoint.Adapter
	address   []netip.Prefix
	router    adapter.Router
	logger    log.ContextLogger
	dnsRouter adapter.DNSRouter
}

func NewEndpoint(ctx context.Context, router adapter.Router, logger log.ContextLogger, tag string, options option.AwgEndpointOptions) (adapter.Endpoint, error) {
	if options.MTU == 0 {
		options.MTU = 1408
	}

	options.UDPFragmentDefault = true
	// Check if any peer has a domain address
	remoteIsDomain := common.Any(options.Peers, func(peer option.AwgPeerOptions) bool {
		return !M.ParseAddr(peer.Address).IsValid()
	})
	dial, err := dialer.NewWithOptions(dialer.Options{
		Context:          ctx,
		Options:          options.DialerOptions,
		RemoteIsDomain:   remoteIsDomain,
		ResolverOnDetour: true,
		DirectOutbound:   true,
	})
	if err != nil {
		return nil, err
	}

	var allowedPrefixBuilder netipx.IPSetBuilder
	var excludedPrefixBuilder netipx.IPSetBuilder
	for _, peer := range options.Peers {
		for _, prefix := range peer.AllowedIPs {
			allowedPrefixBuilder.AddPrefix(prefix)
		}

		if addr, err := netip.ParseAddr(peer.Address); err == nil {
			excludedPrefixBuilder.Add(addr)
		}
	}
	allowedIps, err := allowedPrefixBuilder.IPSet()
	if err != nil {
		return nil, err
	}
	excludedIps, err := excludedPrefixBuilder.IPSet()
	if err != nil {
		return nil, err
	}

	// Create peer resolver function for domain endpoints
	// Always use system resolver for peer endpoints because:
	// 1. VPN server must be resolved before VPN tunnel is established
	// 2. dnsRouter may not be fully initialized at this stage
	var resolvePeer func(domain string) (netip.Addr, error)
	if remoteIsDomain {
		resolvePeer = func(domain string) (netip.Addr, error) {
			addrs, lookupErr := net.DefaultResolver.LookupNetIP(ctx, "ip", domain)
			if lookupErr != nil {
				return netip.Addr{}, lookupErr
			}
			return addrs[0], nil
		}
	}

	ipc, err := genIpcConfig(options, resolvePeer)
	if err != nil {
		return nil, err
	}

	logger.Debug("AWG IPC config:\n", ipc)

	dev, err := awg.NewDevice(ctx, logger, dial, ipc, awg.DeviceOpts{
		UseIntegratedTun: options.UseIntegratedTun,
		Address:          options.Address,
		AllowedIps:       allowedIps.Prefixes(),
		ExcludedIps:      excludedIps.Prefixes(),
		MTU:              options.MTU,
	})
	if err != nil {
		return nil, err
	}

	return &Endpoint{
		Device:    dev,
		Adapter:   endpoint.NewAdapterWithDialerOptions("awg", tag, []string{N.NetworkTCP, N.NetworkUDP}, options.DialerOptions),
		address:   options.Address,
		router:    router,
		logger:    logger,
		dnsRouter: service.FromContext[adapter.DNSRouter](ctx),
	}, nil
}

func genIpcConfig(opts option.AwgEndpointOptions, resolvePeer func(domain string) (netip.Addr, error)) (string, error) {
	privateKeyBytes, err := base64.StdEncoding.DecodeString(opts.PrivateKey)
	if err != nil {
		return "", err
	}
	s := "private_key=" + hex.EncodeToString(privateKeyBytes)
	if opts.ListenPort != 0 {
		s += "\nlisten_port=" + format.ToString(opts.ListenPort)
	}
	if opts.Jc != 0 {
		s += "\njc=" + format.ToString(opts.Jc)
	}
	if opts.Jmin != 0 {
		s += "\njmin=" + format.ToString(opts.Jmin)
	}
	if opts.Jmax != 0 {
		s += "\njmax=" + format.ToString(opts.Jmax)
	}
	if opts.S1 != 0 {
		s += "\ns1=" + format.ToString(opts.S1)
	}
	if opts.S2 != 0 {
		s += "\ns2=" + format.ToString(opts.S2)
	}
	if opts.S3 != 0 {
		s += "\ns3=" + format.ToString(opts.S3)
	}
	if opts.S4 != 0 {
		s += "\ns4=" + format.ToString(opts.S4)
	}
	if opts.H1 != "" {
		s += "\nh1=" + opts.H1
	}
	if opts.H2 != "" {
		s += "\nh2=" + opts.H2
	}
	if opts.H3 != "" {
		s += "\nh3=" + opts.H3
	}
	if opts.H4 != "" {
		s += "\nh4=" + opts.H4
	}
	if opts.I1 != "" {
		s += "\ni1=" + opts.I1
	}
	if opts.I2 != "" {
		s += "\ni2=" + opts.I2
	}
	if opts.I3 != "" {
		s += "\ni3=" + opts.I3
	}
	if opts.I4 != "" {
		s += "\ni4=" + opts.I4
	}
	if opts.I5 != "" {
		s += "\ni5=" + opts.I5
	}

	for _, peer := range opts.Peers {
		publicKeyBytes, err := base64.StdEncoding.DecodeString(peer.PublicKey)
		if err != nil {
			return "", err
		}
		s += "\npublic_key=" + hex.EncodeToString(publicKeyBytes)
		if peer.PresharedKey != "" {
			presharedKeyBytes, err := base64.StdEncoding.DecodeString(peer.PresharedKey)
			if err != nil {
				return "", err
			}
			s += "\npreshared_key=" + hex.EncodeToString(presharedKeyBytes)
		}
		if peer.Address != "" && peer.Port != 0 {
			// Resolve domain to IP if necessary
			endpointAddr := peer.Address
			if addr := M.ParseAddr(peer.Address); !addr.IsValid() {
				// It's a domain, resolve it
				if resolvePeer == nil {
					return "", E.New("peer address is a domain but no resolver provided: ", peer.Address)
				}
				resolvedAddr, resolveErr := resolvePeer(peer.Address)
				if resolveErr != nil {
					return "", E.Cause(resolveErr, "resolve peer endpoint ", peer.Address)
				}
				endpointAddr = resolvedAddr.String()
			}
			s += "\nendpoint=" + endpointAddr + ":" + format.ToString(peer.Port)
		}
		if peer.PersistentKeepaliveInterval != 0 {
			s += "\npersistent_keepalive_interval=" + format.ToString(peer.PersistentKeepaliveInterval)
		}
		for _, allowedIp := range peer.AllowedIPs {
			s += "\nallowed_ip=" + allowedIp.String()
		}
	}
	return s, nil
}

func (e *Endpoint) NewPacketConnectionEx(ctx context.Context, conn N.PacketConn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	var metadata adapter.InboundContext
	metadata.Inbound = e.Tag()
	metadata.InboundType = e.Type()
	metadata.Source = source
	metadata.Destination = destination
	for _, addr := range e.address {
		if addr.Contains(destination.Addr) {
			metadata.OriginDestination = destination
			if destination.Addr.Is4() {
				metadata.Destination.Addr = netip.AddrFrom4([4]uint8{127, 0, 0, 1})
			} else {
				metadata.Destination.Addr = netip.IPv6Loopback()
			}
			conn = bufio.NewNATPacketConn(bufio.NewNetPacketConn(conn), metadata.OriginDestination, metadata.Destination)
		}
	}
	e.logger.InfoContext(ctx, "inbound packet connection from ", source)
	e.logger.InfoContext(ctx, "inbound packet connection to ", destination)
	e.router.RoutePacketConnectionEx(ctx, conn, metadata, onClose)
}

func (e *Endpoint) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	switch network {
	case N.NetworkTCP:
		e.logger.InfoContext(ctx, "outbound connection to ", destination)
	case N.NetworkUDP:
		e.logger.InfoContext(ctx, "outbound packet connection to ", destination)
	}
	if destination.IsFqdn() {
		destinationAddresses, err := e.dnsRouter.Lookup(ctx, destination.Fqdn, adapter.DNSQueryOptions{})
		if err != nil {
			return nil, err
		}
		return N.DialSerial(ctx, e.Device, network, destination, destinationAddresses)
	} else if !destination.Addr.IsValid() {
		return nil, E.New("invalid destination: ", destination)
	}
	return e.Device.DialContext(ctx, network, destination)
}

func (e *Endpoint) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	e.logger.InfoContext(ctx, "outbound packet connection to ", destination)
	if destination.IsFqdn() {
		destinationAddresses, err := e.dnsRouter.Lookup(ctx, destination.Fqdn, adapter.DNSQueryOptions{})
		if err != nil {
			return nil, err
		}
		packetConn, _, err := N.ListenSerial(ctx, e.Device, destination, destinationAddresses)
		if err != nil {
			return nil, err
		}
		return packetConn, nil
	}
	return e.Device.ListenPacket(ctx, destination)
}

func (w *Endpoint) NewConnectionEx(ctx context.Context, conn net.Conn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	var metadata adapter.InboundContext
	metadata.Inbound = w.Tag()
	metadata.InboundType = w.Type()
	metadata.Source = source
	for _, addr := range w.address {
		if addr.Contains(destination.Addr) {
			metadata.OriginDestination = destination
			if destination.Addr.Is4() {
				destination.Addr = netip.AddrFrom4([4]uint8{127, 0, 0, 1})
			} else {
				destination.Addr = netip.IPv6Loopback()
			}
			break
		}
	}
	metadata.Destination = destination
	w.logger.InfoContext(ctx, "inbound connection from ", source)
	w.logger.InfoContext(ctx, "inbound connection to ", metadata.Destination)
	w.router.RouteConnectionEx(ctx, conn, metadata, onClose)
}
