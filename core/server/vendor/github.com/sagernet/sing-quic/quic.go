package qtls

import (
	"context"
	"crypto/tls"
	"net"
	"net/http"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/quic-go/http3"
	M "github.com/sagernet/sing/common/metadata"
	aTLS "github.com/sagernet/sing/common/tls"
)

type Config interface {
	Dial(ctx context.Context, conn net.PacketConn, addr net.Addr, config *quic.Config) (quic.Connection, error)
	DialEarly(ctx context.Context, conn net.PacketConn, addr net.Addr, config *quic.Config) (quic.EarlyConnection, error)
	CreateTransport(conn net.PacketConn, quicConnPtr *quic.EarlyConnection, serverAddr M.Socksaddr, quicConfig *quic.Config) http.RoundTripper
}

type ServerConfig interface {
	Listen(conn net.PacketConn, config *quic.Config) (Listener, error)
	ListenEarly(conn net.PacketConn, config *quic.Config) (EarlyListener, error)
	ConfigureHTTP3()
}

type Listener interface {
	Accept(ctx context.Context) (quic.Connection, error)
	Close() error
	Addr() net.Addr
}

type EarlyListener interface {
	Accept(ctx context.Context) (quic.EarlyConnection, error)
	Close() error
	Addr() net.Addr
}

func Dial(ctx context.Context, conn net.PacketConn, addr net.Addr, config aTLS.Config, quicConfig *quic.Config) (quic.Connection, error) {
	if quicTLSConfig, isQUICConfig := config.(Config); isQUICConfig {
		return quicTLSConfig.Dial(ctx, conn, addr, quicConfig)
	}
	tlsConfig, err := config.Config()
	if err != nil {
		return nil, err
	}
	return quic.Dial(ctx, conn, addr, tlsConfig, quicConfig)
}

func DialEarly(ctx context.Context, conn net.PacketConn, addr net.Addr, config aTLS.Config, quicConfig *quic.Config) (quic.EarlyConnection, error) {
	if quicTLSConfig, isQUICConfig := config.(Config); isQUICConfig {
		return quicTLSConfig.DialEarly(ctx, conn, addr, quicConfig)
	}
	tlsConfig, err := config.Config()
	if err != nil {
		return nil, err
	}
	return quic.DialEarly(ctx, conn, addr, tlsConfig, quicConfig)
}

func CreateTransport(conn net.PacketConn, quicConnPtr *quic.EarlyConnection, serverAddr M.Socksaddr, config aTLS.Config, quicConfig *quic.Config) (http.RoundTripper, error) {
	if quicTLSConfig, isQUICConfig := config.(Config); isQUICConfig {
		return quicTLSConfig.CreateTransport(conn, quicConnPtr, serverAddr, quicConfig), nil
	}
	tlsConfig, err := config.Config()
	if err != nil {
		return nil, err
	}
	return &http3.Transport{
		TLSClientConfig: tlsConfig,
		QUICConfig:      quicConfig,
		Dial: func(ctx context.Context, addr string, tlsCfg *tls.Config, cfg *quic.Config) (quic.EarlyConnection, error) {
			quicConn, err := quic.DialEarly(ctx, conn, serverAddr.UDPAddr(), tlsCfg, cfg)
			if err != nil {
				return nil, err
			}
			*quicConnPtr = quicConn
			return quicConn, nil
		},
	}, nil
}

func Listen(conn net.PacketConn, config aTLS.ServerConfig, quicConfig *quic.Config) (Listener, error) {
	if quicTLSConfig, isQUICConfig := config.(ServerConfig); isQUICConfig {
		return quicTLSConfig.Listen(conn, quicConfig)
	}
	tlsConfig, err := config.Config()
	if err != nil {
		return nil, err
	}
	return quic.Listen(conn, tlsConfig, quicConfig)
}

func ListenEarly(conn net.PacketConn, config aTLS.ServerConfig, quicConfig *quic.Config) (EarlyListener, error) {
	if quicTLSConfig, isQUICConfig := config.(ServerConfig); isQUICConfig {
		return quicTLSConfig.ListenEarly(conn, quicConfig)
	}
	tlsConfig, err := config.Config()
	if err != nil {
		return nil, err
	}
	return quic.ListenEarly(conn, tlsConfig, quicConfig)
}

func ConfigureHTTP3(config aTLS.ServerConfig) error {
	if len(config.NextProtos()) == 0 {
		config.SetNextProtos([]string{http3.NextProtoH3})
	}
	if quicTLSConfig, isQUICConfig := config.(ServerConfig); isQUICConfig {
		quicTLSConfig.ConfigureHTTP3()
		return nil
	}
	tlsConfig, err := config.Config()
	if err != nil {
		return err
	}
	http3.ConfigureTLSConfig(tlsConfig)
	return nil
}
