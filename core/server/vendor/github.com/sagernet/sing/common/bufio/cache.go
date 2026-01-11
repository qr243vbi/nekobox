package bufio

import (
	"io"
	"net"

	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type CachedConn struct {
	net.Conn
	buffer *buf.Buffer
}

func NewCachedConn(conn net.Conn, buffer *buf.Buffer) *CachedConn {
	buffer.IncRef()
	return &CachedConn{
		Conn:   conn,
		buffer: buffer,
	}
}

func (c *CachedConn) ReadCached() *buf.Buffer {
	buffer := c.buffer
	c.buffer = nil
	if buffer != nil {
		buffer.DecRef()
	}
	return buffer
}

func (c *CachedConn) Read(p []byte) (n int, err error) {
	if c.buffer != nil {
		n, err = c.buffer.Read(p)
		if err == nil {
			return
		}
		c.buffer.DecRef()
		c.buffer.Release()
		c.buffer = nil
	}
	return c.Conn.Read(p)
}

func (c *CachedConn) WriteTo(w io.Writer) (n int64, err error) {
	if c.buffer != nil {
		wn, wErr := w.Write(c.buffer.Bytes())
		if wErr != nil {
			c.buffer.DecRef()
			c.buffer.Release()
			c.buffer = nil
		}
		n += int64(wn)
	}
	cn, err := Copy(w, c.Conn)
	n += cn
	return
}

func (c *CachedConn) ReadFrom(r io.Reader) (n int64, err error) {
	return Copy(c.Conn, r)
}

func (c *CachedConn) Upstream() any {
	return c.Conn
}

func (c *CachedConn) ReaderReplaceable() bool {
	return c.buffer == nil
}

func (c *CachedConn) WriterReplaceable() bool {
	return true
}

func (c *CachedConn) Close() error {
	if buffer := c.buffer; buffer != nil {
		buffer.DecRef()
		buffer.Release()
		c.buffer = nil
	}
	return c.Conn.Close()
}

type CachedReader struct {
	upstream io.Reader
	buffer   *buf.Buffer
}

func NewCachedReader(upstream io.Reader, buffer *buf.Buffer) *CachedReader {
	buffer.IncRef()
	return &CachedReader{
		upstream: upstream,
		buffer:   buffer,
	}
}

func (r *CachedReader) ReadCached() *buf.Buffer {
	buffer := r.buffer
	r.buffer = nil
	if buffer != nil {
		buffer.DecRef()
	}
	return buffer
}

func (r *CachedReader) Read(p []byte) (n int, err error) {
	if r.buffer != nil {
		n, err = r.buffer.Read(p)
		if err == nil {
			return
		}
		r.buffer.DecRef()
		r.buffer.Release()
		r.buffer = nil
	}
	return r.upstream.Read(p)
}

func (r *CachedReader) WriteTo(w io.Writer) (n int64, err error) {
	if r.buffer != nil {
		wn, wErr := w.Write(r.buffer.Bytes())
		if wErr != nil {
			return 0, wErr
		}
		n += int64(wn)
	}
	cn, err := Copy(w, r.upstream)
	n += cn
	return
}

func (r *CachedReader) Upstream() any {
	return r.upstream
}

func (r *CachedReader) ReaderReplaceable() bool {
	return r.buffer == nil
}

func (r *CachedReader) Close() error {
	if buffer := r.buffer; buffer != nil {
		buffer.DecRef()
		buffer.Release()
		r.buffer = nil
	}
	return nil
}

type CachedPacketConn struct {
	N.PacketConn
	buffer      *buf.Buffer
	destination M.Socksaddr
}

func NewCachedPacketConn(conn N.PacketConn, buffer *buf.Buffer, destination M.Socksaddr) *CachedPacketConn {
	buffer.IncRef()
	return &CachedPacketConn{
		PacketConn:  conn,
		buffer:      buffer,
		destination: destination,
	}
}

func (c *CachedPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	if c.buffer != nil {
		_, err = buffer.ReadOnceFrom(c.buffer)
		if err != nil {
			return M.Socksaddr{}, err
		}
		c.buffer.DecRef()
		c.buffer.Release()
		c.buffer = nil
		return c.destination, nil
	}
	return c.PacketConn.ReadPacket(buffer)
}

func (c *CachedPacketConn) ReadCachedPacket() *N.PacketBuffer {
	buffer := c.buffer
	c.buffer = nil
	if buffer != nil {
		buffer.DecRef()
	}
	packet := N.NewPacketBuffer()
	*packet = N.PacketBuffer{
		Buffer:      buffer,
		Destination: c.destination,
	}
	return packet
}

func (c *CachedPacketConn) Upstream() any {
	return c.PacketConn
}

func (c *CachedPacketConn) ReaderReplaceable() bool {
	return c.buffer == nil
}

func (c *CachedPacketConn) WriterReplaceable() bool {
	return true
}

func (c *CachedPacketConn) Close() error {
	if buffer := c.buffer; buffer != nil {
		buffer.DecRef()
		buffer.Release()
		c.buffer = nil
	}
	return c.PacketConn.Close()
}
