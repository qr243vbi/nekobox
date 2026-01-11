package shadowtls

import (
	"context"
	"crypto/tls"
	"net"

	"github.com/sagernet/sing/common"

	utls "github.com/metacubex/utls"
)

type (
	TLSSessionIDGeneratorFunc func(clientHello []byte, sessionID []byte) error

	TLSHandshakeFunc func(
		ctx context.Context,
		conn net.Conn,
		sessionIDGenerator TLSSessionIDGeneratorFunc, // for shadow-tls version 3
	) error
)

func DefaultTLSHandshakeFunc(password string, config *tls.Config) TLSHandshakeFunc {
	return func(ctx context.Context, conn net.Conn, sessionIDGenerator TLSSessionIDGeneratorFunc) error {
		tlsConfig := &utls.Config{
			Rand:                  config.Rand,
			Time:                  config.Time,
			VerifyPeerCertificate: config.VerifyPeerCertificate,
			RootCAs:               config.RootCAs,
			NextProtos:            config.NextProtos,
			ServerName:            config.ServerName,
			InsecureSkipVerify:    config.InsecureSkipVerify,
			CipherSuites:          config.CipherSuites,
			MinVersion:            config.MinVersion,
			MaxVersion:            config.MaxVersion,
			CurvePreferences: common.Map(config.CurvePreferences, func(it tls.CurveID) utls.CurveID {
				return utls.CurveID(it)
			}),
			SessionTicketsDisabled: config.SessionTicketsDisabled,
			Renegotiation:          utls.RenegotiationSupport(config.Renegotiation),
			SessionIDGenerator:     sessionIDGenerator,
		}
		tlsConn := utls.Client(conn, tlsConfig)
		return tlsConn.HandshakeContext(ctx)
	}
}
