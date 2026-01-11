package uot

import (
	"net"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	N "github.com/sagernet/sing/common/network"
)

type LazyClientConn struct {
	net.Conn
	writer         N.VectorisedWriter
	request        Request
	requestWritten bool
}

func NewLazyClientConn(conn net.Conn, request Request) *LazyClientConn {
	return &LazyClientConn{
		Conn:    conn,
		request: request,
		writer:  bufio.NewVectorisedWriter(conn),
	}
}

func NewLazyConn(conn net.Conn, request Request) *Conn {
	clientConn := NewLazyClientConn(conn, request)
	return NewConn(clientConn, request)
}

func (c *LazyClientConn) Write(p []byte) (n int, err error) {
	if !c.requestWritten {
		var request *buf.Buffer
		request, err = EncodeRequest(c.request)
		if err != nil {
			return
		}
		err = c.writer.WriteVectorised([]*buf.Buffer{request, buf.As(p)})
		if err != nil {
			return
		}
		c.requestWritten = true
		return len(p), nil
	}
	return c.Conn.Write(p)
}

func (c *LazyClientConn) WriteVectorised(buffers []*buf.Buffer) error {
	if !c.requestWritten {
		request, err := EncodeRequest(c.request)
		if err != nil {
			return err
		}

		err = c.writer.WriteVectorised(append([]*buf.Buffer{request}, buffers...))
		c.requestWritten = true
		return err
	}
	return c.writer.WriteVectorised(buffers)
}

func (c *LazyClientConn) NeedHandshake() bool {
	return !c.requestWritten
}

func (c *LazyClientConn) ReaderReplaceable() bool {
	return true
}

func (c *LazyClientConn) WriterReplaceable() bool {
	return c.requestWritten
}

func (c *LazyClientConn) Upstream() any {
	return c.Conn
}
