package tls

import (
	"context"
	"net"
	"sync"
)

type Listener struct {
	net.Listener
	config ServerConfig
}

func NewListener(inner net.Listener, config ServerConfig) net.Listener {
	return &Listener{
		Listener: inner,
		config:   config,
	}
}

func (l *Listener) Accept() (net.Conn, error) {
	conn, err := l.Listener.Accept()
	if err != nil {
		return nil, err
	}
	return NewLazyConn(conn, l.config), nil
}

type LazyConn struct {
	net.Conn
	tlsConfig     ServerConfig
	access        sync.Mutex
	needHandshake bool
}

func NewLazyConn(conn net.Conn, config ServerConfig) *LazyConn {
	return &LazyConn{
		Conn:          conn,
		tlsConfig:     config,
		needHandshake: true,
	}
}

func (c *LazyConn) HandshakeContext(ctx context.Context) error {
	if !c.needHandshake {
		return nil
	}
	c.access.Lock()
	defer c.access.Unlock()
	if c.needHandshake {
		tlsConn, err := ServerHandshake(ctx, c.Conn, c.tlsConfig)
		if err != nil {
			return err
		}
		c.Conn = tlsConn
		c.needHandshake = false
	}
	return nil
}

func (c *LazyConn) Read(p []byte) (n int, err error) {
	err = c.HandshakeContext(context.Background())
	if err != nil {
		return
	}
	return c.Conn.Read(p)
}

func (c *LazyConn) Write(p []byte) (n int, err error) {
	err = c.HandshakeContext(context.Background())
	if err != nil {
		return
	}
	return c.Conn.Write(p)
}

func (c *LazyConn) NeedHandshake() bool {
	return c.needHandshake
}

func (c *LazyConn) ReaderReplaceable() bool {
	return !c.needHandshake
}

func (c *LazyConn) WriterReplaceable() bool {
	return !c.needHandshake
}

func (c *LazyConn) Upstream() any {
	return c.Conn
}
