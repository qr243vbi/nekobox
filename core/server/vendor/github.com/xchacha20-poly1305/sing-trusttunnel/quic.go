//go:build with_quic

package trusttunnel

import (
	"context"
	stdTLS "crypto/tls"
	"net"
	"runtime"
	"time"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/quic-go/congestion"
	"github.com/sagernet/quic-go/http3"
	"github.com/sagernet/sing-quic"
	"github.com/sagernet/sing-quic/congestion_bbr1"
	"github.com/sagernet/sing-quic/congestion_bbr2"
	congestion_meta1 "github.com/sagernet/sing-quic/congestion_meta1"
	congestion_meta2 "github.com/sagernet/sing-quic/congestion_meta2"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/tls"
)

func (c *Client) quicRoundTripper(tlsConfig tls.Config, congestionControlName string) error {
	stdConfig, err := tlsConfig.STDConfig()
	if err != nil {
		return err
	}
	c.roundTripper = &http3.Transport{
		TLSClientConfig: stdConfig,
		QUICConfig: &quic.Config{
			Versions:                   []quic.Version{quic.Version1},
			MaxIdleTimeout:             DefaultQuicMaxIdleTimeout,
			InitialStreamReceiveWindow: DefaultQuicStreamReceiveWindow,
			DisablePathMTUDiscovery:    !(runtime.GOOS == "windows" || runtime.GOOS == "linux" || runtime.GOOS == "android" || runtime.GOOS == "darwin"),
			Allow0RTT:                  false,
		},
		Dial: func(ctx context.Context, addr string, tlsCfg *stdTLS.Config, cfg *quic.Config) (*quic.Conn, error) {
			packetConn, err := c.detour.ListenPacket(ctx, c.server)
			if err != nil {
				return nil, err
			}
			quicConn, err := quic.DialEarly(ctx, packetConn, c.server.UDPAddr(), tlsCfg, cfg)
			if err != nil {
				_ = packetConn.Close()
				return nil, err
			}
			setCongestionControl(c.timeFunc, quicConn, congestionControlName)
			return quicConn, nil
		},
	}
	c.wrapError = qtls.WrapError
	return nil
}

func (s *Service) configHTTP3Server(tlsConfig tls.ServerConfig, udpConn net.PacketConn) error {
	err := qtls.ConfigureHTTP3(tlsConfig)
	if err != nil {
		return err
	}
	quicListener, err := qtls.ListenEarly(udpConn, tlsConfig, &quic.Config{
		Versions:           []quic.Version{quic.Version1},
		MaxIdleTimeout:     DefaultQuicMaxIdleTimeout,
		MaxIncomingStreams: 1 << 60,
		Allow0RTT:          true,
	})
	if err != nil {
		return err
	}
	h3Server := &http3.Server{
		Handler:     s,
		IdleTimeout: DefaultSessionTimeout,
		ConnContext: func(ctx context.Context, conn *quic.Conn) context.Context {
			setCongestionControl(s.timeFunc, conn, s.quicCongestionControl)
			ctx = contextWithWrapError(ctx, qtls.WrapError)
			return ctx
		},
	}
	s.h3Server = h3Server
	s.udpConn = udpConn
	go func() {
		sErr := h3Server.ServeListener(quicListener)
		if sErr != nil && !E.IsClosedOrCanceled(sErr) {
			s.logger.ErrorContext(s.ctx, "HTTP3 server close: ", sErr)
		}
	}()
	return nil
}

func setCongestionControl(timeFunc func() time.Time, conn *quic.Conn, name string) {
	var congestionControl congestion.CongestionControl
	switch name {
	case "bbr_standard":
		congestionControl = congestion_bbr1.NewBbrSender(
			congestion_bbr1.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(conn.Config().InitialPacketSize),
			congestion_bbr1.InitialCongestionWindowPackets,
			congestion_bbr1.MaxCongestionWindowPackets,
		)
	case "bbr2":
		congestionControl = congestion_bbr2.NewBBR2Sender(
			congestion_bbr2.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(conn.Config().InitialPacketSize),
			0,
			false,
		)
	case "bbr_variant":
		congestionControl = congestion_bbr2.NewBBR2Sender(
			congestion_bbr2.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(conn.Config().InitialPacketSize),
			32*congestion.ByteCount(conn.Config().InitialPacketSize),
			true,
		)
	case "cubic":
		congestionControl = congestion_meta1.NewCubicSender(
			congestion_meta1.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(conn.Config().InitialPacketSize),
			false,
		)
	case "reno":
		congestionControl = congestion_meta1.NewCubicSender(
			congestion_meta1.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(conn.Config().InitialPacketSize),
			true,
		)
	case "", "bbr":
		fallthrough
	default:
		congestionControl = congestion_meta2.NewBbrSender(
			congestion_meta2.DefaultClock{TimeFunc: timeFunc},
			congestion.ByteCount(conn.Config().InitialPacketSize),
			congestion.ByteCount(congestion_meta1.InitialCongestionWindow),
		)
	}
	conn.SetCongestionControl(congestionControl)
}
