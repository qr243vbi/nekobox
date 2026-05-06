package vless

import (
	"context"
	stdtls "crypto/tls"
	"encoding/base64"
	"net"
	"reflect"
	"strings"
	"sync"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/adapter/outbound"
	"github.com/sagernet/sing-box/common/dialer"
	"github.com/sagernet/sing-box/common/mux"
	"github.com/sagernet/sing-box/common/tls"
	"github.com/sagernet/sing-box/common/vision"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing-box/protocol/vless/encryption"
	"github.com/sagernet/sing-box/transport/v2ray"
	"github.com/sagernet/sing-vmess/packetaddr"
	"github.com/sagernet/sing-vmess/vless"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

func RegisterOutbound(registry *outbound.Registry) {
	outbound.Register[option.VLESSOutboundOptions](registry, C.TypeVLESS, NewOutbound)
}

type Outbound struct {
	outbound.Adapter
	logger          logger.ContextLogger
	dialer          N.Dialer
	client          *vless.Client
	serverAddr      M.Socksaddr
	multiplexDialer *mux.Client
	tlsConfig       tls.Config
	tlsDialer       tls.Dialer
	transport       adapter.V2RayClientTransport
	packetAddr      bool
	xudp            bool
	encryption      *encryption.ClientInstance
	vision          bool
}

func NewOutbound(ctx context.Context, router adapter.Router, logger log.ContextLogger, tag string, options option.VLESSOutboundOptions) (adapter.Outbound, error) {
	outboundDialer, err := dialer.New(ctx, options.DialerOptions, options.ServerIsDomain())
	if err != nil {
		return nil, err
	}
	outbound := &Outbound{
		Adapter:    outbound.NewAdapterWithDialerOptions(C.TypeVLESS, tag, options.Network.Build(), options.DialerOptions),
		logger:     logger,
		dialer:     outboundDialer,
		serverAddr: options.ServerOptions.Build(),
		vision:     strings.HasPrefix(options.Flow, "xtls-rprx-vision"),
	}
	if options.TLS != nil {
		outbound.tlsConfig, err = tls.NewClientWithOptions(tls.ClientOptions{
			Context:       ctx,
			Logger:        logger,
			ServerAddress: options.Server,
			Options:       common.PtrValueOrDefault(options.TLS),
			KTLSCompatible: common.PtrValueOrDefault(options.Transport).Type == "" &&
				!common.PtrValueOrDefault(options.Multiplex).Enabled &&
				options.Flow == "",
		})
		if err != nil {
			return nil, err
		}
		outbound.tlsDialer = tls.NewDialer(outboundDialer, outbound.tlsConfig)
	}
	if options.Encryption != "" && options.Encryption != "none" {
		encryptionConfig, err := parseClientEncryption(options.Encryption)
		if err != nil {
			return nil, E.Cause(err, "parse encryption")
		}
		outbound.encryption = &encryption.ClientInstance{}
		if err := outbound.encryption.Init(encryptionConfig.keys, encryptionConfig.xorMode, encryptionConfig.seconds, encryptionConfig.padding); err != nil {
			return nil, E.Cause(err, "initialize encryption")
		}
		logger.Debug("encryption initialized: keys=", len(encryptionConfig.keys), " xorMode=", encryptionConfig.xorMode, " seconds=", encryptionConfig.seconds, " padding=", encryptionConfig.padding)
	}
	if options.Transport != nil {
		outbound.transport, err = v2ray.NewClientTransport(ctx, outbound.dialer, outbound.serverAddr, common.PtrValueOrDefault(options.Transport), outbound.tlsConfig)
		if err != nil {
			return nil, E.Cause(err, "create client transport: ", options.Transport.Type)
		}
	}
	if options.PacketEncoding == nil {
		outbound.xudp = true
	} else {
		switch *options.PacketEncoding {
		case "":
		case "packetaddr":
			outbound.packetAddr = true
		case "xudp":
			outbound.xudp = true
		default:
			return nil, E.New("unknown packet encoding: ", options.PacketEncoding)
		}
	}
	outbound.client, err = vless.NewClient(options.UUID, options.Flow, logger)
	if err != nil {
		return nil, err
	}
	outbound.multiplexDialer, err = mux.NewClientWithOptions((*vlessDialer)(outbound), logger, common.PtrValueOrDefault(options.Multiplex))
	if err != nil {
		return nil, err
	}
	return outbound, nil
}

func (h *Outbound) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	if h.multiplexDialer == nil {
		switch N.NetworkName(network) {
		case N.NetworkTCP:
			h.logger.InfoContext(ctx, "outbound connection to ", destination)
		case N.NetworkUDP:
			h.logger.InfoContext(ctx, "outbound packet connection to ", destination)
		}
		return (*vlessDialer)(h).DialContext(ctx, network, destination)
	} else {
		switch N.NetworkName(network) {
		case N.NetworkTCP:
			h.logger.InfoContext(ctx, "outbound multiplex connection to ", destination)
		case N.NetworkUDP:
			h.logger.InfoContext(ctx, "outbound multiplex packet connection to ", destination)
		}
		return h.multiplexDialer.DialContext(ctx, network, destination)
	}
}

func (h *Outbound) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	if h.multiplexDialer == nil {
		h.logger.InfoContext(ctx, "outbound packet connection to ", destination)
		return (*vlessDialer)(h).ListenPacket(ctx, destination)
	} else {
		h.logger.InfoContext(ctx, "outbound multiplex packet connection to ", destination)
		return h.multiplexDialer.ListenPacket(ctx, destination)
	}
}

func (h *Outbound) InterfaceUpdated() {
	if h.transport != nil {
		h.transport.Close()
	}
	if h.multiplexDialer != nil {
		h.multiplexDialer.Reset()
	}
}

func (h *Outbound) Close() error {
	return common.Close(common.PtrOrNil(h.multiplexDialer), h.transport)
}

type vlessDialer Outbound

func (h *vlessDialer) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	ctx, metadata := adapter.ExtendContext(ctx)
	metadata.Outbound = h.Tag()
	metadata.Destination = destination
	var conn net.Conn
	var baseConn net.Conn
	var hookOnce sync.Once
	if h.vision {
		ctx = vision.WithHook(ctx, func(tlsConn net.Conn) {
			if tlsConn == nil || !isVisionTLSConn(tlsConn) {
				return
			}
			hookOnce.Do(func() {
				baseConn = tlsConn
			})
		})
	}
	var err error
	if h.transport != nil {
		conn, err = h.transport.DialContext(ctx)
		if err == nil && h.vision {
			if baseConn == nil {
				// Only set baseConn if the transport delivered a TLS-capable connection
				if isVisionTLSConn(conn) {
					h.logger.Warn("Vision enabled but hook was not called by transport, using fallback")
					baseConn = conn
				}
			}
		}
	} else if h.tlsDialer != nil {
		conn, err = h.tlsDialer.DialTLSContext(ctx, h.serverAddr)
	} else {
		conn, err = h.dialer.DialContext(ctx, N.NetworkTCP, h.serverAddr)
	}
	if err == nil && h.vision && baseConn == nil && h.transport == nil {
		baseConn = conn
	}
	if err != nil {
		return nil, err
	}

	// Apply encryption if configured
	if h.encryption != nil {
		conn, err = h.encryption.Handshake(conn)
		if err != nil {
			return nil, E.Cause(err, "encryption handshake")
		}
	}

	// For Vision: wrap the connection to expose the TLS/encryption connection for vless client
	var visionBaseConn net.Conn // The connection to pass to Vision (TLS or encryption layer)
	var visionCanSplice bool
	if h.vision {
		isRAWTransport := h.transport == nil

		if baseConn != nil && !isVisionTLSConn(baseConn) {
			baseConn = nil
		}
		if baseConn != nil {
			// Has TLS/Reality: use baseConn (TLS connection)
			visionBaseConn = baseConn
			visionCanSplice = isRAWTransport
			conn = newVisionConnWrapper(conn, baseConn)
		} else if h.encryption != nil {
			// Only has encryption (no TLS/Reality): use encryption layer itself
			encConn := findEncryptionLayer(conn)
			if encConn != nil {
				visionBaseConn = encConn
				if h.encryption.IsFullRandomXorMode() {
					visionCanSplice = false
				} else {
					visionCanSplice = isRAWTransport
				}
				conn = newVisionConnWrapper(conn, encConn)
			} else {
				return nil, E.New("Vision: failed to find encryption layer")
			}
		} else {
			return nil, E.New("Vision requires either TLS/Reality or Encryption")
		}
	}
	switch N.NetworkName(network) {
	case N.NetworkTCP:
		h.logger.InfoContext(ctx, "outbound connection to ", destination)
		if h.vision && visionBaseConn != nil {
			// For Vision, we need to pass the base connection (TLS or encryption layer)
			// to prepareConn so it can properly initialize VisionConn
			return h.client.DialEarlyConnWithOptions(conn, visionBaseConn, destination, visionCanSplice)
		}
		return h.client.DialEarlyConn(conn, destination)
	case N.NetworkUDP:
		h.logger.InfoContext(ctx, "outbound packet connection to ", destination)
		if h.xudp {
			return h.client.DialEarlyXUDPPacketConn(conn, destination)
		} else if h.packetAddr {
			if destination.IsDomain() {
				return nil, E.New("packetaddr: domain destination is not supported")
			}
			packetConn, err := h.client.DialEarlyPacketConn(conn, M.Socksaddr{Fqdn: packetaddr.SeqPacketMagicAddress})
			if err != nil {
				return nil, err
			}
			return bufio.NewBindPacketConn(packetaddr.NewConn(packetConn, destination), destination), nil
		} else {
			return h.client.DialEarlyPacketConn(conn, destination)
		}
	default:
		return nil, E.Extend(N.ErrUnknownNetwork, network)
	}
}

func (h *vlessDialer) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	h.logger.InfoContext(ctx, "outbound packet connection to ", destination)
	ctx, metadata := adapter.ExtendContext(ctx)
	metadata.Outbound = h.Tag()
	metadata.Destination = destination
	var conn net.Conn
	var err error
	if h.transport != nil {
		conn, err = h.transport.DialContext(ctx)
	} else if h.tlsDialer != nil {
		conn, err = h.tlsDialer.DialTLSContext(ctx, h.serverAddr)
	} else {
		conn, err = h.dialer.DialContext(ctx, N.NetworkTCP, h.serverAddr)
	}
	if err != nil {
		common.Close(conn)
		return nil, err
	}
	// Apply encryption if configured
	if h.encryption != nil {
		conn, err = h.encryption.Handshake(conn)
		if err != nil {
			common.Close(conn)
			return nil, E.Cause(err, "encryption handshake")
		}
	}
	if h.xudp {
		return h.client.DialEarlyXUDPPacketConn(conn, destination)
	} else if h.packetAddr {
		if destination.IsDomain() {
			return nil, E.New("packetaddr: domain destination is not supported")
		}
		conn, err := h.client.DialEarlyPacketConn(conn, M.Socksaddr{Fqdn: packetaddr.SeqPacketMagicAddress})
		if err != nil {
			return nil, err
		}
		return packetaddr.NewConn(conn, destination), nil
	} else {
		return h.client.DialEarlyPacketConn(conn, destination)
	}
}

type visionConnWrapper struct {
	net.Conn
	upstream net.Conn
}

var (
	_ N.ReaderWithUpstream = (*visionConnWrapper)(nil)
	_ N.WriterWithUpstream = (*visionConnWrapper)(nil)
	_ common.WithUpstream  = (*visionConnWrapper)(nil)
)

func newVisionConnWrapper(conn net.Conn, upstream net.Conn) net.Conn {
	if upstream == nil || conn == nil || conn == upstream {
		return conn
	}
	return &visionConnWrapper{
		Conn:     conn,
		upstream: upstream,
	}
}

func (c *visionConnWrapper) Upstream() any {
	return c.upstream
}

func (c *visionConnWrapper) ReaderReplaceable() bool {
	if replacer, ok := c.Conn.(N.ReaderWithUpstream); ok {
		return replacer.ReaderReplaceable()
	}
	return true
}

func (c *visionConnWrapper) WriterReplaceable() bool {
	if replacer, ok := c.Conn.(N.WriterWithUpstream); ok {
		return replacer.WriterReplaceable()
	}
	return true
}

// isVisionTLSConn returns true when the provided connection exposes TLS semantics Vision expects.
func isVisionTLSConn(conn net.Conn) bool {
	if conn == nil {
		return false
	}
	if _, ok := conn.(interface{ ConnectionState() stdtls.ConnectionState }); ok {
		return true
	}
	if _, ok := conn.(interface{ Handshake() error }); ok {
		return true
	}
	connType := reflect.TypeOf(conn)
	if connType == nil {
		return false
	}
	if connType.Kind() == reflect.Ptr {
		pkgPath := connType.Elem().PkgPath()
		if pkgPath == "crypto/tls" || strings.Contains(pkgPath, "utls") || strings.Contains(pkgPath, "shadowtls") {
			return true
		}
	}
	return false
}

func findEncryptionLayer(conn net.Conn) net.Conn {
	for conn != nil {
		if enc, ok := conn.(encryption.EncryptionConn); ok && enc.IsEncryptionLayer() {
			return conn
		}
		if upstream, ok := conn.(common.WithUpstream); ok {
			if next := upstream.Upstream(); next != nil {
				if nextConn, ok := next.(net.Conn); ok {
					conn = nextConn
					continue
				}
			}
		}
		break
	}
	return nil
}

type clientEncryptionConfig struct {
	keys    [][]byte
	xorMode uint32
	seconds uint32
	padding string
}

func parseClientEncryption(raw string) (clientEncryptionConfig, error) {
	var cfg clientEncryptionConfig
	raw = strings.TrimSpace(raw)
	if raw == "" {
		return cfg, E.New("empty encryption string")
	}
	parts := strings.Split(raw, ".")
	if len(parts) < 4 {
		return cfg, E.New("invalid encryption string: missing components")
	}
	if parts[0] != "mlkem768x25519plus" {
		return cfg, E.New("unsupported encryption prefix: ", parts[0])
	}
	switch parts[1] {
	case "native":
		cfg.xorMode = 0
	case "xorpub":
		cfg.xorMode = 1
	case "random":
		cfg.xorMode = 2
	default:
		return cfg, E.New("unknown encryption mode: ", parts[1])
	}
	switch parts[2] {
	case "0rtt":
		cfg.seconds = 1
	case "1rtt":
		cfg.seconds = 0
	default:
		return cfg, E.New("unsupported encryption RTT value: ", parts[2])
	}
	paddingPhase := true
	var paddingParts []string
	for _, segment := range parts[3:] {
		segment = strings.TrimSpace(segment)
		if segment == "" {
			return cfg, E.New("invalid empty segment in encryption string")
		}
		if data, err := base64.RawURLEncoding.DecodeString(segment); err == nil {
			if len(data) == 32 || len(data) == 1184 {
				cfg.keys = append(cfg.keys, data)
				paddingPhase = false
				continue
			}
			return cfg, E.New("invalid encryption key length: ", len(data))
		}
		if paddingPhase {
			paddingParts = append(paddingParts, segment)
			continue
		}
		return cfg, E.New("invalid encryption key: ", segment)
	}
	if len(cfg.keys) == 0 {
		return cfg, E.New("no valid encryption keys found in encryption string")
	}
	if len(paddingParts) > 0 {
		cfg.padding = strings.Join(paddingParts, ".")
	}
	return cfg, nil
}
