package tls

import (
	"context"
	"crypto/tls"
	"net"
)

type (
	STDConfig       = tls.Config
	STDConn         = tls.Conn
	ConnectionState = tls.ConnectionState
)

type Config interface {
	ServerName() string
	SetServerName(serverName string)
	NextProtos() []string
	SetNextProtos(nextProto []string)
	Config() (*STDConfig, error)
	Client(conn net.Conn) (Conn, error)
	Clone() Config
}

type ConfigCompat interface {
	Config
	ClientHandshake(ctx context.Context, conn net.Conn) (Conn, error)
}

type ServerConfig interface {
	Config
	Start() error
	Close() error
	Server(conn net.Conn) (Conn, error)
}

type ServerConfigCompat interface {
	ServerConfig
	ServerHandshake(ctx context.Context, conn net.Conn) (Conn, error)
}

type WithSessionIDGenerator interface {
	SetSessionIDGenerator(generator func(clientHello []byte, sessionID []byte) error)
}

type Conn interface {
	net.Conn
	NetConn() net.Conn
	HandshakeContext(ctx context.Context) error
	ConnectionState() ConnectionState
}

func ClientHandshake(ctx context.Context, conn net.Conn, config Config) (Conn, error) {
	if compatServer, isCompat := config.(ConfigCompat); isCompat {
		return compatServer.ClientHandshake(ctx, conn)
	}
	tlsConn, err := config.Client(conn)
	if err != nil {
		return nil, err
	}
	err = tlsConn.HandshakeContext(ctx)
	if err != nil {
		return nil, err
	}
	return tlsConn, nil
}

func ServerHandshake(ctx context.Context, conn net.Conn, config ServerConfig) (Conn, error) {
	if compatServer, isCompat := config.(ServerConfigCompat); isCompat {
		return compatServer.ServerHandshake(ctx, conn)
	}
	tlsConn, err := config.Server(conn)
	if err != nil {
		return nil, err
	}
	err = tlsConn.HandshakeContext(ctx)
	if err != nil {
		return nil, err
	}
	return tlsConn, nil
}
