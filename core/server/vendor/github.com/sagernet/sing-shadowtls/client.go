package shadowtls

import (
	"context"
	"crypto/hmac"
	"crypto/sha1"
	"encoding/hex"
	"net"
	"os"

	"github.com/sagernet/sing/common/debug"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type ClientConfig struct {
	Version      int
	Password     string
	Server       M.Socksaddr
	Dialer       N.Dialer
	StrictMode   bool
	TLSHandshake TLSHandshakeFunc
	Logger       logger.ContextLogger
}

type Client struct {
	version      int
	password     string
	strictMode   bool
	server       M.Socksaddr
	dialer       N.Dialer
	tlsHandshake TLSHandshakeFunc
	logger       logger.ContextLogger
}

func NewClient(config ClientConfig) (*Client, error) {
	client := &Client{
		version:      config.Version,
		password:     config.Password,
		strictMode:   config.StrictMode,
		server:       config.Server,
		dialer:       config.Dialer,
		tlsHandshake: config.TLSHandshake,
		logger:       config.Logger,
	}

	switch client.version {
	case 1, 2, 3:
	default:
		return nil, E.New("unknown protocol version: ", client.version)
	}
	if client.dialer == nil {
		client.dialer = N.SystemDialer
	}
	return client, nil
}

func (c *Client) SetHandshakeFunc(handshakeFunc TLSHandshakeFunc) {
	c.tlsHandshake = handshakeFunc
}

func (c *Client) DialContext(ctx context.Context) (net.Conn, error) {
	if !c.server.IsValid() {
		return nil, os.ErrInvalid
	}
	conn, err := c.dialer.DialContext(ctx, N.NetworkTCP, c.server)
	if err != nil {
		return nil, err
	}
	shadowTLSConn, err := c.DialContextConn(ctx, conn)
	if err != nil {
		conn.Close()
		return nil, err
	}
	return shadowTLSConn, nil
}

func (c *Client) DialContextConn(ctx context.Context, conn net.Conn) (net.Conn, error) {
	if c.tlsHandshake == nil {
		return nil, os.ErrInvalid
	}
	switch c.version {
	default:
		fallthrough
	case 1:
		err := c.tlsHandshake(ctx, conn, nil)
		if err != nil {
			return nil, err
		}
		c.logger.TraceContext(ctx, "clint handshake finished")
		return conn, nil
	case 2:
		hashConn := newHashReadConn(conn, c.password)
		err := c.tlsHandshake(ctx, hashConn, nil)
		if err != nil {
			return nil, err
		}
		c.logger.TraceContext(ctx, "clint handshake finished")
		return newClientConn(hashConn), nil
	case 3:
		stream := newStreamWrapper(conn, c.password)
		err := c.tlsHandshake(ctx, stream, generateSessionID(c.password))
		if err != nil {
			return nil, err
		}
		c.logger.TraceContext(ctx, "handshake success")
		isTLS13, authorized, serverRandom, readHMAC := stream.Authorized()
		if c.strictMode && !isTLS13 {
			return nil, E.New("TLS1.3 is not supported")
		} else if !authorized {
			return nil, E.New("traffic hijacked")
		}
		if debug.Enabled {
			c.logger.TraceContext(ctx, "authorized, server random extracted: ", hex.EncodeToString(serverRandom))
		}
		hmacAdd := hmac.New(sha1.New, []byte(c.password))
		hmacAdd.Write(serverRandom)
		hmacAdd.Write([]byte("C"))
		hmacVerify := hmac.New(sha1.New, []byte(c.password))
		hmacVerify.Write(serverRandom)
		hmacVerify.Write([]byte("S"))
		return newVerifiedConn(conn, hmacAdd, hmacVerify, readHMAC), nil
	}
}
