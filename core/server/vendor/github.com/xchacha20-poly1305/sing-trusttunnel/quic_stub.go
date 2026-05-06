//go:build !with_quic

package trusttunnel

import (
	"net"

	"github.com/sagernet/sing/common/tls"
)

func (c *Client) quicRoundTripper(tlsConfig tls.Config, congestionControlName string) error {
	return ErrQUICNotIncluded
}

func (s *Service) configHTTP3Server(tlsConfig tls.ServerConfig, udpConn net.PacketConn) error {
	return ErrQUICNotIncluded
}
