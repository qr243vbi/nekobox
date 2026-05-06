package vless

import (
	"context"
	"encoding/base64"
	"net"
	"os"
	"strconv"
	"strings"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/adapter/inbound"
	"github.com/sagernet/sing-box/common/listener"
	"github.com/sagernet/sing-box/common/mux"
	"github.com/sagernet/sing-box/common/tls"
	"github.com/sagernet/sing-box/common/uot"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing-box/protocol/vless/encryption"
	"github.com/sagernet/sing-box/transport/v2ray"
	"github.com/sagernet/sing-vmess/packetaddr"
	"github.com/sagernet/sing-vmess/vless"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/auth"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

func RegisterInbound(registry *inbound.Registry) {
	inbound.Register[option.VLESSInboundOptions](registry, C.TypeVLESS, NewInbound)
}

var _ adapter.TCPInjectableInbound = (*Inbound)(nil)

type Inbound struct {
	inbound.Adapter
	ctx        context.Context
	router     adapter.ConnectionRouterEx
	logger     logger.ContextLogger
	listener   *listener.Listener
	users      []option.VLESSUser
	service    *vless.Service[int]
	tlsConfig  tls.ServerConfig
	transport  adapter.V2RayServerTransport
	decryption *encryption.ServerInstance
}

func NewInbound(ctx context.Context, router adapter.Router, logger log.ContextLogger, tag string, options option.VLESSInboundOptions) (adapter.Inbound, error) {
	inbound := &Inbound{
		Adapter: inbound.NewAdapter(C.TypeVLESS, tag),
		ctx:     ctx,
		router:  uot.NewRouter(router, logger),
		logger:  logger,
		users:   options.Users,
	}
	var err error
	inbound.router, err = mux.NewRouterWithOptions(inbound.router, logger, common.PtrValueOrDefault(options.Multiplex))
	if err != nil {
		return nil, err
	}
	service := vless.NewService[int](logger, adapter.NewUpstreamContextHandlerEx(inbound.newConnectionEx, inbound.newPacketConnectionEx))
	service.UpdateUsers(common.MapIndexed(inbound.users, func(index int, _ option.VLESSUser) int {
		return index
	}), common.Map(inbound.users, func(it option.VLESSUser) string {
		return it.UUID
	}), common.Map(inbound.users, func(it option.VLESSUser) string {
		return it.Flow
	}))
	inbound.service = service
	if options.TLS != nil {
		inbound.tlsConfig, err = tls.NewServerWithOptions(tls.ServerOptions{
			Context: ctx,
			Logger:  logger,
			Options: common.PtrValueOrDefault(options.TLS),
			KTLSCompatible: common.PtrValueOrDefault(options.Transport).Type == "" &&
				!common.PtrValueOrDefault(options.Multiplex).Enabled &&
				common.All(options.Users, func(it option.VLESSUser) bool {
					return it.Flow == ""
				}),
		})
		if err != nil {
			return nil, err
		}
	}
	if options.Transport != nil {
		inbound.transport, err = v2ray.NewServerTransport(ctx, logger, common.PtrValueOrDefault(options.Transport), inbound.tlsConfig, (*inboundTransportHandler)(inbound))
		if err != nil {
			return nil, E.Cause(err, "create server transport: ", options.Transport.Type)
		}
	}
	// Parse decryption configuration
	if options.Decryption != "" && options.Decryption != "none" {
		decryptionConfig, err := parseServerDecryption(options.Decryption)
		if err != nil {
			return nil, E.Cause(err, "parse decryption")
		}
		inbound.decryption = &encryption.ServerInstance{}
		if err := inbound.decryption.Init(decryptionConfig.keys, decryptionConfig.xorMode, decryptionConfig.secondsFrom, decryptionConfig.secondsTo, decryptionConfig.padding); err != nil {
			return nil, E.Cause(err, "initialize decryption")
		}
		logger.Debug("decryption initialized with ", len(decryptionConfig.keys), " keys xorMode=", decryptionConfig.xorMode, " secondsFrom=", decryptionConfig.secondsFrom, " secondsTo=", decryptionConfig.secondsTo, " padding=", decryptionConfig.padding)
	}
	inbound.listener = listener.New(listener.Options{
		Context:           ctx,
		Logger:            logger,
		Network:           []string{N.NetworkTCP},
		Listen:            options.ListenOptions,
		ConnectionHandler: inbound,
	})
	return inbound, nil
}

func (h *Inbound) Start(stage adapter.StartStage) error {
	if stage != adapter.StartStateStart {
		return nil
	}
	if h.tlsConfig != nil {
		err := h.tlsConfig.Start()
		if err != nil {
			return err
		}
	}
	if h.transport == nil {
		return h.listener.Start()
	}
	if common.Contains(h.transport.Network(), N.NetworkTCP) {
		tcpListener, err := h.listener.ListenTCP()
		if err != nil {
			return err
		}
		go func() {
			sErr := h.transport.Serve(tcpListener)
			if sErr != nil && !E.IsClosed(sErr) {
				h.logger.Error("transport serve error: ", sErr)
			}
		}()
	}
	if common.Contains(h.transport.Network(), N.NetworkUDP) {
		udpConn, err := h.listener.ListenUDP()
		if err != nil {
			return err
		}
		go func() {
			sErr := h.transport.ServePacket(udpConn)
			if sErr != nil && !E.IsClosed(sErr) {
				h.logger.Error("transport serve error: ", sErr)
			}
		}()
	}
	return nil
}

func (h *Inbound) Close() error {
	if h.decryption != nil {
		h.decryption.Close()
	}
	return common.Close(
		h.service,
		h.listener,
		h.tlsConfig,
		h.transport,
	)
}

func (h *Inbound) NewConnectionEx(ctx context.Context, conn net.Conn, metadata adapter.InboundContext, onClose N.CloseHandlerFunc) {
	canSplice := h.transport == nil
	if canSplice && h.decryption != nil && h.decryption.IsFullRandomXorMode() {
		canSplice = false
	}
	h.newConnectionExInternal(ctx, conn, metadata, onClose, canSplice)
}

func (h *Inbound) newConnectionExInternal(ctx context.Context, conn net.Conn, metadata adapter.InboundContext, onClose N.CloseHandlerFunc, canSplice bool) {
	if h.tlsConfig != nil && h.transport == nil {
		tlsConn, err := tls.ServerHandshake(ctx, conn, h.tlsConfig)
		if err != nil {
			N.CloseOnHandshakeFailure(conn, onClose, err)
			h.logger.ErrorContext(ctx, E.Cause(err, "process connection from ", metadata.Source, ": TLS handshake"))
			return
		}
		conn = tlsConn
	}
	if h.decryption != nil {
		encConn, err := h.decryption.Handshake(conn, nil)
		if err != nil {
			N.CloseOnHandshakeFailure(conn, onClose, err)
			h.logger.ErrorContext(ctx, E.Cause(err, "process connection from ", metadata.Source, ": encryption handshake"))
			return
		}
		conn = encConn
	}
	err := h.service.NewConnectionWithOptions(adapter.WithContext(ctx, &metadata), conn, metadata.Source, onClose, canSplice)
	if err != nil {
		N.CloseOnHandshakeFailure(conn, onClose, err)
		h.logger.ErrorContext(ctx, E.Cause(err, "process connection from ", metadata.Source))
	}
}

func (h *Inbound) newConnectionEx(ctx context.Context, conn net.Conn, metadata adapter.InboundContext, onClose N.CloseHandlerFunc) {
	metadata.Inbound = h.Tag()
	metadata.InboundType = h.Type()
	userIndex, loaded := auth.UserFromContext[int](ctx)
	if !loaded {
		N.CloseOnHandshakeFailure(conn, onClose, os.ErrInvalid)
		return
	}
	user := h.users[userIndex].Name
	if user == "" {
		user = F.ToString(userIndex)
	} else {
		metadata.User = user
	}
	h.logger.InfoContext(ctx, "[", user, "] inbound connection to ", metadata.Destination)
	h.router.RouteConnectionEx(ctx, conn, metadata, onClose)
}

func (h *Inbound) newPacketConnectionEx(ctx context.Context, conn N.PacketConn, metadata adapter.InboundContext, onClose N.CloseHandlerFunc) {
	metadata.Inbound = h.Tag()
	metadata.InboundType = h.Type()
	userIndex, loaded := auth.UserFromContext[int](ctx)
	if !loaded {
		N.CloseOnHandshakeFailure(conn, onClose, os.ErrInvalid)
		return
	}
	user := h.users[userIndex].Name
	if user == "" {
		user = F.ToString(userIndex)
	} else {
		metadata.User = user
	}
	if metadata.Destination.Fqdn == packetaddr.SeqPacketMagicAddress {
		metadata.Destination = M.Socksaddr{}
		conn = packetaddr.NewConn(bufio.NewNetPacketConn(conn), metadata.Destination)
		h.logger.InfoContext(ctx, "[", user, "] inbound packet addr connection")
	} else {
		h.logger.InfoContext(ctx, "[", user, "] inbound packet connection to ", metadata.Destination)
	}
	h.router.RoutePacketConnectionEx(ctx, conn, metadata, onClose)
}

type serverDecryptionConfig struct {
	keys        [][]byte
	xorMode     uint32
	secondsFrom int64
	secondsTo   int64
	padding     string
}

func parseServerDecryption(raw string) (serverDecryptionConfig, error) {
	var cfg serverDecryptionConfig
	raw = strings.TrimSpace(raw)
	if raw == "" {
		return cfg, E.New("empty decryption string")
	}
	parts := strings.Split(raw, ".")
	if len(parts) < 4 {
		return cfg, E.New("invalid decryption string: missing components")
	}
	if parts[0] != "mlkem768x25519plus" {
		return cfg, E.New("unsupported decryption prefix: ", parts[0])
	}
	switch parts[1] {
	case "native":
		cfg.xorMode = 0
	case "xorpub":
		cfg.xorMode = 1
	case "random":
		cfg.xorMode = 2
	default:
		return cfg, E.New("unknown decryption mode: ", parts[1])
	}

	secondsToken := strings.TrimSpace(parts[2])
	if secondsToken == "" {
		return cfg, E.New("invalid decryption seconds segment")
	}
	trimmed := strings.TrimSuffix(secondsToken, "s")
	if trimmed == "" {
		return cfg, E.New("invalid decryption seconds segment")
	}
	values := strings.SplitN(trimmed, "-", 2)
	secondsFrom, err := strconv.ParseInt(values[0], 10, 64)
	if err != nil {
		return cfg, E.Cause(err, "parse decryption seconds_from")
	}
	cfg.secondsFrom = secondsFrom
	if len(values) == 2 && values[1] != "" {
		secondsTo, err := strconv.ParseInt(values[1], 10, 64)
		if err != nil {
			return cfg, E.Cause(err, "parse decryption seconds_to")
		}
		cfg.secondsTo = secondsTo
	}

	paddingPhase := true
	var paddingParts []string
	for _, segment := range parts[3:] {
		segment = strings.TrimSpace(segment)
		if segment == "" {
			return cfg, E.New("invalid empty segment in decryption string")
		}
		if data, err := base64.RawURLEncoding.DecodeString(segment); err == nil {
			if len(data) == 32 || len(data) == 64 {
				cfg.keys = append(cfg.keys, data)
				paddingPhase = false
				continue
			}
			return cfg, E.New("invalid decryption key length: ", len(data))
		}
		if paddingPhase {
			paddingParts = append(paddingParts, segment)
			continue
		}
		return cfg, E.New("invalid decryption key: ", segment)
	}
	if len(cfg.keys) == 0 {
		return cfg, E.New("no valid decryption keys found in decryption string")
	}
	if len(paddingParts) > 0 {
		cfg.padding = strings.Join(paddingParts, ".")
	}
	return cfg, nil
}

var _ adapter.V2RayServerTransportHandler = (*inboundTransportHandler)(nil)

type inboundTransportHandler Inbound

func (h *inboundTransportHandler) NewConnectionEx(ctx context.Context, conn net.Conn, source M.Socksaddr, destination M.Socksaddr, onClose N.CloseHandlerFunc) {
	var metadata adapter.InboundContext
	metadata.Source = source
	metadata.Destination = destination
	//nolint:staticcheck
	metadata.InboundDetour = h.listener.ListenOptions().Detour
	//nolint:staticcheck
	h.logger.InfoContext(ctx, "inbound connection from ", metadata.Source)
	(*Inbound)(h).NewConnectionEx(ctx, conn, metadata, onClose)
}
