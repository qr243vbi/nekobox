package mux

import (
	"context"
	"io"
	"net"
	"os"
	"time"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/baderror"
	M "github.com/sagernet/sing/common/metadata"
)

type httpConn struct {
	reader io.Reader
	writer io.Writer
	create chan struct{}
	err    error
	cancel context.CancelFunc
}

func newHTTPConn(reader io.Reader, writer io.Writer) *httpConn {
	return &httpConn{
		reader: reader,
		writer: writer,
	}
}

func newLateHTTPConn(writer io.Writer, cancel context.CancelFunc) *httpConn {
	return &httpConn{
		create: make(chan struct{}),
		writer: writer,
		cancel: cancel,
	}
}

func (c *httpConn) setup(reader io.Reader, err error) {
	c.reader = reader
	c.err = err
	close(c.create)
}

func (c *httpConn) Read(b []byte) (n int, err error) {
	if c.reader == nil {
		<-c.create
		if c.err != nil {
			return 0, c.err
		}
	}
	n, err = c.reader.Read(b)
	return n, baderror.WrapH2(err)
}

func (c *httpConn) Write(b []byte) (n int, err error) {
	n, err = c.writer.Write(b)
	return n, baderror.WrapH2(err)
}

func (c *httpConn) Close() error {
	if c.cancel != nil {
		c.cancel()
	}
	return common.Close(c.reader, c.writer)
}

func (c *httpConn) LocalAddr() net.Addr {
	return M.Socksaddr{}
}

func (c *httpConn) RemoteAddr() net.Addr {
	return M.Socksaddr{}
}

func (c *httpConn) SetDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *httpConn) SetReadDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *httpConn) SetWriteDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *httpConn) NeedAdditionalReadDeadline() bool {
	return true
}
