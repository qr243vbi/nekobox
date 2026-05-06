package deadline

import (
	"net"
	"time"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	N "github.com/sagernet/sing/common/network"
)

type Conn struct {
	N.ExtendedConn
	reader Reader
}

func NewConn(conn net.Conn) N.ExtendedConn {
	if deadlineConn, isDeadline := conn.(*Conn); isDeadline {
		return deadlineConn
	}
	return NewSerialConn(&Conn{ExtendedConn: bufio.NewExtendedConn(conn), reader: NewReader(conn)})
}

func NewFallbackConn(conn net.Conn) N.ExtendedConn {
	if deadlineConn, isDeadline := conn.(*Conn); isDeadline {
		return deadlineConn
	}
	return NewSerialConn(&Conn{ExtendedConn: bufio.NewExtendedConn(conn), reader: NewFallbackReader(conn)})
}

func (c *Conn) Read(p []byte) (n int, err error) {
	return c.reader.Read(p)
}

func (c *Conn) ReadBuffer(buffer *buf.Buffer) error {
	return c.reader.ReadBuffer(buffer)
}

func (c *Conn) SetReadDeadline(t time.Time) error {
	return c.reader.SetReadDeadline(t)
}

func (c *Conn) ReaderReplaceable() bool {
	return c.reader.ReaderReplaceable()
}

func (c *Conn) UpstreamReader() any {
	return c.reader.UpstreamReader()
}

func (c *Conn) WriterReplaceable() bool {
	return true
}

func (c *Conn) Upstream() any {
	return c.ExtendedConn
}

func (c *Conn) NeedAdditionalReadDeadline() bool {
	return false
}
