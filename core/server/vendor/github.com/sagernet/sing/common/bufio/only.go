package bufio

import (
	"io"
	"net"
	"time"
)

type ReadOnlyConn struct {
	reader io.Reader
}

func NewReadOnlyConn(reader io.Reader) net.Conn {
	return &ReadOnlyConn{reader}
}

func (c *ReadOnlyConn) Read(b []byte) (n int, err error) {
	return c.reader.Read(b)
}

func (c *ReadOnlyConn) Write(b []byte) (n int, err error) {
	return 0, io.ErrClosedPipe
}

func (c *ReadOnlyConn) Close() error {
	return nil
}

func (c *ReadOnlyConn) LocalAddr() net.Addr {
	return nil
}

func (c *ReadOnlyConn) RemoteAddr() net.Addr {
	return nil
}

func (c *ReadOnlyConn) SetDeadline(t time.Time) error {
	return nil
}

func (c *ReadOnlyConn) SetWriteDeadline(t time.Time) error {
	return nil
}

func (c *ReadOnlyConn) SetReadDeadline(t time.Time) error {
	return nil
}

func (c *ReadOnlyConn) Upstream() any {
	return c.reader
}

type WriteOnlyConn struct {
	writer io.Writer
}

func NewWriteOnlyConn(writer io.Writer) net.Conn {
	return &WriteOnlyConn{writer}
}

func (c *WriteOnlyConn) Read(b []byte) (n int, err error) {
	return 0, io.ErrClosedPipe
}

func (c *WriteOnlyConn) Write(b []byte) (n int, err error) {
	return c.writer.Write(b)
}

func (c *WriteOnlyConn) Close() error {
	return nil
}

func (c *WriteOnlyConn) LocalAddr() net.Addr {
	return nil
}

func (c *WriteOnlyConn) RemoteAddr() net.Addr {
	return nil
}

func (c *WriteOnlyConn) SetDeadline(t time.Time) error {
	return nil
}

func (c *WriteOnlyConn) SetWriteDeadline(t time.Time) error {
	return nil
}

func (c *WriteOnlyConn) SetReadDeadline(t time.Time) error {
	return nil
}

func (c *WriteOnlyConn) Upstream() any {
	return c.writer
}
