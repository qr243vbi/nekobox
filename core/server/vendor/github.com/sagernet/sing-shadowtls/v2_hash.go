package shadowtls

import (
	"crypto/hmac"
	"crypto/sha1"
	"hash"
	"net"
)

type hashReadConn struct {
	net.Conn
	hmac hash.Hash
}

func newHashReadConn(conn net.Conn, password string) *hashReadConn {
	return &hashReadConn{
		conn,
		hmac.New(sha1.New, []byte(password)),
	}
}

func (c *hashReadConn) Read(b []byte) (n int, err error) {
	n, err = c.Conn.Read(b)
	if err != nil {
		return
	}
	_, err = c.hmac.Write(b[:n])
	return
}

func (c *hashReadConn) Sum() []byte {
	return c.hmac.Sum(nil)[:8]
}

type hashWriteConn struct {
	net.Conn
	hmac       hash.Hash
	hasContent bool
	lastSum    []byte
}

func newHashWriteConn(conn net.Conn, password string) *hashWriteConn {
	return &hashWriteConn{
		Conn: conn,
		hmac: hmac.New(sha1.New, []byte(password)),
	}
}

func (c *hashWriteConn) Write(p []byte) (n int, err error) {
	if c.hmac != nil {
		if c.hasContent {
			c.lastSum = c.Sum()
		}
		c.hmac.Write(p)
		c.hasContent = true
	}
	return c.Conn.Write(p)
}

func (c *hashWriteConn) Sum() []byte {
	return c.hmac.Sum(nil)[:8]
}

func (c *hashWriteConn) LastSum() []byte {
	return c.lastSum
}

func (c *hashWriteConn) Fallback() {
	c.hmac = nil
}

func (c *hashWriteConn) HasContent() bool {
	return c.hasContent
}
