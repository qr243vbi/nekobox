package uot

import (
	"net"
	"sync"
	"sync/atomic"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	N "github.com/sagernet/sing/common/network"
)

type LazyClientConn struct {
	net.Conn
	writer         N.VectorisedWriter
	request        Request
	access         sync.Mutex
	requestWritten atomic.Bool
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
	if c.requestWritten.Load() {
		return c.Conn.Write(p)
	}
	err = c.writeVectorised([]*buf.Buffer{buf.As(p)})
	if err != nil {
		return 0, err
	}
	return len(p), nil
}

func (c *LazyClientConn) WriteVectorised(buffers []*buf.Buffer) error {
	if c.requestWritten.Load() {
		return c.writer.WriteVectorised(buffers)
	}
	return c.writeVectorised(buffers)
}

func (c *LazyClientConn) writeVectorised(buffers []*buf.Buffer) error {
	c.access.Lock()
	defer c.access.Unlock()

	if c.requestWritten.Load() {
		return c.writer.WriteVectorised(buffers)
	}

	request, err := EncodeRequest(c.request)
	if err != nil {
		buf.ReleaseMulti(buffers)
		return err
	}
	err = c.writer.WriteVectorised(append([]*buf.Buffer{request}, buffers...))
	if err != nil {
		return err
	}
	c.requestWritten.Store(true)
	return nil
}

func (c *LazyClientConn) NeedHandshake() bool {
	return !c.requestWritten.Load()
}

func (c *LazyClientConn) ReaderReplaceable() bool {
	return true
}

func (c *LazyClientConn) WriterReplaceable() bool {
	return c.requestWritten.Load()
}

func (c *LazyClientConn) Upstream() any {
	return c.Conn
}
