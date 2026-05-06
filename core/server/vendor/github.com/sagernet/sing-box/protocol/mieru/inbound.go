package mieru

import (
	"context"
	"fmt"
	"io"
	"net"
	"net/netip"
	"sync"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/adapter/inbound"
	"github.com/sagernet/sing-box/common/listener"
	"github.com/sagernet/sing-box/common/uot"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"

	mierucommon "github.com/enfein/mieru/v3/apis/common"
	mieruconstant "github.com/enfein/mieru/v3/apis/constant"
	mierumodel "github.com/enfein/mieru/v3/apis/model"
	mieruserver "github.com/enfein/mieru/v3/apis/server"
	mierutp "github.com/enfein/mieru/v3/apis/trafficpattern"
	mierupb "github.com/enfein/mieru/v3/pkg/appctl/appctlpb"
	"google.golang.org/protobuf/proto"
)

func RegisterInbound(registry *inbound.Registry) {
	inbound.Register[option.MieruInboundOptions](registry, C.TypeMieru, NewInbound)
}

type Inbound struct {
	inbound.Adapter
	ctx       context.Context
	router    adapter.ConnectionRouterEx
	logger    log.ContextLogger
	listener  *listener.Listener
	server    mieruserver.Server
	userNames []string

	mu sync.Mutex
}

func NewInbound(ctx context.Context, router adapter.Router, logger log.ContextLogger, tag string, options option.MieruInboundOptions) (adapter.Inbound, error) {
	config, userNames, err := buildMieruServerConfig(ctx, options)
	if err != nil {
		return nil, fmt.Errorf("failed to build mieru server config: %w", err)
	}

	s := mieruserver.NewServer()
	if err := s.Store(config); err != nil {
		return nil, fmt.Errorf("failed to store mieru server config: %w", err)
	}

	inboundInstance := &Inbound{
		Adapter:   inbound.NewAdapter(C.TypeMieru, tag),
		ctx:       ctx,
		router:    uot.NewRouter(router, logger),
		logger:    logger,
		server:    s,
		userNames: userNames,
	}
	inboundInstance.listener = listener.New(listener.Options{
		Context: ctx,
		Logger:  logger,
		Network: []string{N.NetworkTCP, N.NetworkUDP},
		Listen:  options.ListenOptions,
	})

	return inboundInstance, nil
}

func (h *Inbound) Start(stage adapter.StartStage) error {
	if stage != adapter.StartStateStart {
		return nil
	}

	h.mu.Lock()
	defer h.mu.Unlock()

	if err := h.server.Start(); err != nil {
		return fmt.Errorf("failed to start mieru server: %w", err)
	}

	h.logger.Info("mieru server is started")
	go h.acceptLoop()
	return nil
}

func (h *Inbound) Close() error {
	h.mu.Lock()
	defer h.mu.Unlock()

	if h.server.IsRunning() {
		return h.server.Stop()
	}
	return nil
}

func (h *Inbound) acceptLoop() {
	for {
		conn, request, err := h.server.Accept()
		if err != nil {
			if !h.server.IsRunning() {
				return
			}
			h.logger.Debug("failed to accept mieru connection: ", err)
			continue
		}
		go h.handleConnection(conn, request)
	}
}

func (h *Inbound) handleConnection(conn net.Conn, request *mierumodel.Request) {
	ctx := log.ContextWithNewID(h.ctx)

	// Send fake SOCKS5 response back to proxy client.
	resp := &mierumodel.Response{
		Reply: mieruconstant.Socks5ReplySuccess,
		BindAddr: mierumodel.AddrSpec{
			IP:   net.IPv4zero,
			Port: 0,
		},
	}
	if err := resp.WriteToSocks5(conn); err != nil {
		conn.Close()
		h.logger.DebugContext(ctx, "failed to write mieru response: ", err)
		return
	}

	// Build metadata.
	var metadata adapter.InboundContext
	metadata.Inbound = h.Tag()
	metadata.InboundType = h.Type()
	//nolint:staticcheck
	metadata.InboundDetour = h.listener.ListenOptions().Detour
	metadata.UDPDisableDomainUnmapping = h.listener.ListenOptions().UDPDisableDomainUnmapping

	// Parse source address.
	if remoteAddr := conn.RemoteAddr(); remoteAddr != nil {
		metadata.Source = M.SocksaddrFromNet(remoteAddr)
	}

	// Parse destination from request.
	if request.DstAddr.FQDN != "" {
		metadata.Destination = M.Socksaddr{
			Fqdn: request.DstAddr.FQDN,
			Port: uint16(request.DstAddr.Port),
		}
	} else if request.DstAddr.IP != nil {
		addr, _ := netip.AddrFromSlice(request.DstAddr.IP)
		metadata.Destination = M.Socksaddr{
			Addr: addr.Unmap(),
			Port: uint16(request.DstAddr.Port),
		}
	}

	// Get username from connection.
	if userCtx, ok := conn.(mierucommon.UserContext); ok {
		metadata.User = userCtx.UserName()
	}

	// Handle request.
	switch request.Command {
	case mieruconstant.Socks5ConnectCmd:
		h.logger.InfoContext(ctx, "inbound TCP connection from ", metadata.Source, " to ", metadata.Destination)
		if metadata.User != "" {
			h.logger.InfoContext(ctx, "[", metadata.User, "] inbound TCP connection")
		}
		h.router.RouteConnectionEx(ctx, conn, metadata, nil)
	case mieruconstant.Socks5UDPAssociateCmd:
		h.logger.InfoContext(ctx, "inbound UDP connection from ", metadata.Source, " to ", metadata.Destination)
		if metadata.User != "" {
			h.logger.InfoContext(ctx, "[", metadata.User, "] inbound UDP connection")
		}
		h.handleUDP(ctx, conn, metadata)
	default:
		conn.Close()
		h.logger.WarnContext(ctx, "unsupported mieru command: ", request.Command)
	}
}

func (h *Inbound) handleUDP(ctx context.Context, conn net.Conn, metadata adapter.InboundContext) {
	pc := mierucommon.NewPacketOverStreamTunnel(conn)
	packetConn := &mieruPacketConn{
		PacketConn:  pc,
		destination: metadata.Destination,
	}
	h.router.RoutePacketConnectionEx(ctx, packetConn, metadata, nil)
}

// mieruPacketConn wraps mieru's PacketConn to implement N.PacketConn
type mieruPacketConn struct {
	net.PacketConn
	destination M.Socksaddr
}

var _ N.PacketConn = (*mieruPacketConn)(nil)

// ReadPacket parses the SOCKS5 UDP header and returns the destination address.
func (c *mieruPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	n, _, err := c.PacketConn.ReadFrom(buffer.FreeBytes())
	if err != nil {
		return M.Socksaddr{}, err
	}
	buffer.Truncate(n)
	if buffer.Len() < 3 {
		return M.Socksaddr{}, io.ErrShortBuffer
	}

	// Skip RSV (2 bytes) and FRAG (1 byte).
	buffer.Advance(3)

	var addr mierumodel.AddrSpec
	if err := addr.ReadFromSocks5(buffer); err != nil {
		return M.Socksaddr{}, err
	}
	if addr.FQDN != "" {
		destination = M.Socksaddr{
			Fqdn: addr.FQDN,
			Port: uint16(addr.Port),
		}
	} else if addr.IP != nil {
		netAddr, _ := netip.AddrFromSlice(addr.IP)
		destination = M.Socksaddr{
			Addr: netAddr.Unmap(),
			Port: uint16(addr.Port),
		}
	}
	return destination, nil
}

// WritePacket writes the SOCKS5 UDP header and the payload.
func (c *mieruPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	header := buf.NewSize(3 + M.MaxSocksaddrLength)
	defer header.Release()

	// RSV (2 bytes) + FRAG (1 byte)
	common.Must(header.WriteZeroN(3))

	var addr mierumodel.AddrSpec
	if destination.IsFqdn() {
		addr.FQDN = destination.Fqdn
	} else {
		addr.IP = destination.Addr.AsSlice()
	}
	addr.Port = int(destination.Port)
	if err := addr.WriteToSocks5(header); err != nil {
		return err
	}

	packet := buf.NewSize(header.Len() + buffer.Len())
	defer packet.Release()
	common.Must1(packet.Write(header.Bytes()))
	common.Must1(packet.Write(buffer.Bytes()))
	_, err := c.PacketConn.WriteTo(packet.Bytes(), nil)
	return err
}

func buildMieruServerConfig(_ context.Context, options option.MieruInboundOptions) (*mieruserver.ServerConfig, []string, error) {
	if err := validateMieruInboundOptions(options); err != nil {
		return nil, nil, fmt.Errorf("failed to validate mieru options: %w", err)
	}

	var transportProtocol *mierupb.TransportProtocol
	switch options.Transport {
	case "TCP":
		transportProtocol = mierupb.TransportProtocol_TCP.Enum()
	case "UDP":
		transportProtocol = mierupb.TransportProtocol_UDP.Enum()
	}

	if options.ListenOptions.ListenPort == 0 {
		return nil, nil, E.New("listen_port must be set")
	}
	portBindings := []*mierupb.PortBinding{
		{
			Port:     proto.Int32(int32(options.ListenOptions.ListenPort)),
			Protocol: transportProtocol,
		},
	}

	var users []*mierupb.User
	var userNames []string
	for _, user := range options.Users {
		users = append(users, &mierupb.User{
			Name:     proto.String(user.Name),
			Password: proto.String(user.Password),
		})
		userNames = append(userNames, user.Name)
	}
	var trafficPattern *mierupb.TrafficPattern
	trafficPattern, _ = mierutp.Decode(options.TrafficPattern)
	return &mieruserver.ServerConfig{
		Config: &mierupb.ServerConfig{
			PortBindings:   portBindings,
			Users:          users,
			TrafficPattern: trafficPattern,
		},
	}, userNames, nil
}

func validateMieruInboundOptions(options option.MieruInboundOptions) error {
	if options.Transport != "TCP" && options.Transport != "UDP" {
		return E.New("transport must be TCP or UDP")
	}
	if len(options.Users) == 0 {
		return E.New("users is empty")
	}
	for _, user := range options.Users {
		if user.Name == "" {
			return E.New("username is empty")
		}
		if user.Password == "" {
			return E.New("password is empty")
		}
	}
	if options.TrafficPattern != "" {
		trafficPattern, err := mierutp.Decode(options.TrafficPattern)
		if err != nil {
			return fmt.Errorf("failed to decode traffic pattern %q: %w", options.TrafficPattern, err)
		}
		if err := mierutp.Validate(trafficPattern); err != nil {
			return fmt.Errorf("invalid traffic pattern %q: %w", options.TrafficPattern, err)
		}
	}
	return nil
}
