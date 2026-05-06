package socks

import (
	"net"
	"os"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	M "github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/protocol/socks/socks4"
	"github.com/sagernet/sing/protocol/socks/socks5"
)

type LazyConn struct {
	net.Conn
	socksVersion    byte
	responseWritten bool
}

func NewLazyConn(conn net.Conn, socksVersion byte) *LazyConn {
	return &LazyConn{
		Conn:         conn,
		socksVersion: socksVersion,
	}
}

func (c *LazyConn) ConnHandshakeSuccess(conn net.Conn) error {
	if c.responseWritten {
		return nil
	}
	defer func() {
		c.responseWritten = true
	}()
	switch c.socksVersion {
	case socks4.Version:
		return socks4.WriteResponse(c.Conn, socks4.Response{
			ReplyCode:   socks4.ReplyCodeGranted,
			Destination: M.SocksaddrFromNet(conn.LocalAddr()).Unwrap(),
		})
	case socks5.Version:
		return socks5.WriteResponse(c.Conn, socks5.Response{
			ReplyCode: socks5.ReplyCodeSuccess,
			Bind:      M.SocksaddrFromNet(conn.LocalAddr()).Unwrap(),
		})
	default:
		panic("unknown socks version")
	}
}

func (c *LazyConn) HandshakeFailure(err error) error {
	if c.responseWritten {
		return os.ErrInvalid
	}
	defer func() {
		c.responseWritten = true
	}()
	switch c.socksVersion {
	case socks4.Version:
		return socks4.WriteResponse(c.Conn, socks4.Response{
			ReplyCode: socks4.ReplyCodeRejectedOrFailed,
		})
	case socks5.Version:
		return socks5.WriteResponse(c.Conn, socks5.Response{
			ReplyCode: socks5.ReplyCodeForError(err),
		})
	default:
		panic("unknown socks version")
	}
}

func (c *LazyConn) Read(p []byte) (n int, err error) {
	if !c.responseWritten {
		err = c.ConnHandshakeSuccess(c.Conn)
		if err != nil {
			return
		}
	}
	return c.Conn.Read(p)
}

func (c *LazyConn) Write(p []byte) (n int, err error) {
	if !c.responseWritten {
		err = c.ConnHandshakeSuccess(c.Conn)
		if err != nil {
			return
		}
	}
	return c.Conn.Write(p)
}

func (c *LazyConn) ReaderReplaceable() bool {
	return c.responseWritten
}

func (c *LazyConn) WriterReplaceable() bool {
	return c.responseWritten
}

func (c *LazyConn) Upstream() any {
	return c.Conn
}

type LazyAssociatePacketConn struct {
	AssociatePacketConn
	responseWritten bool
}

func NewLazyAssociatePacketConn(conn net.Conn, underlying net.Conn) *LazyAssociatePacketConn {
	return &LazyAssociatePacketConn{
		AssociatePacketConn: AssociatePacketConn{
			AbstractConn: conn,
			conn:         bufio.NewExtendedConn(conn),
			underlying:   underlying,
		},
	}
}

func (c *LazyAssociatePacketConn) HandshakeSuccess() error {
	if c.responseWritten {
		return nil
	}
	defer func() {
		c.responseWritten = true
	}()
	return socks5.WriteResponse(c.underlying, socks5.Response{
		ReplyCode: socks5.ReplyCodeSuccess,
		Bind:      M.SocksaddrFromNet(c.conn.LocalAddr()).Unwrap(),
	})
}

func (c *LazyAssociatePacketConn) HandshakeFailure(err error) error {
	if c.responseWritten {
		return os.ErrInvalid
	}
	defer func() {
		c.responseWritten = true
		c.conn.Close()
		c.underlying.Close()
	}()
	return socks5.WriteResponse(c.underlying, socks5.Response{
		ReplyCode: socks5.ReplyCodeForError(err),
	})
}

func (c *LazyAssociatePacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	if !c.responseWritten {
		err = c.HandshakeSuccess()
		if err != nil {
			return
		}
	}
	return c.AssociatePacketConn.ReadFrom(p)
}

func (c *LazyAssociatePacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	if !c.responseWritten {
		err = c.HandshakeSuccess()
		if err != nil {
			return
		}
	}
	return c.AssociatePacketConn.WriteTo(p, addr)
}

func (c *LazyAssociatePacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	if !c.responseWritten {
		err = c.HandshakeSuccess()
		if err != nil {
			return
		}
	}
	return c.AssociatePacketConn.ReadPacket(buffer)
}

func (c *LazyAssociatePacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	if !c.responseWritten {
		err := c.HandshakeSuccess()
		if err != nil {
			return err
		}
	}
	return c.AssociatePacketConn.WritePacket(buffer, destination)
}

func (c *LazyAssociatePacketConn) Read(p []byte) (n int, err error) {
	if !c.responseWritten {
		err = c.HandshakeSuccess()
		if err != nil {
			return
		}
	}
	return c.AssociatePacketConn.Read(p)
}

func (c *LazyAssociatePacketConn) Write(p []byte) (n int, err error) {
	if !c.responseWritten {
		err = c.HandshakeSuccess()
		if err != nil {
			return
		}
	}
	return c.AssociatePacketConn.Write(p)
}

func (c *LazyAssociatePacketConn) ReaderReplaceable() bool {
	return c.responseWritten
}

func (c *LazyAssociatePacketConn) WriterReplaceable() bool {
	return c.responseWritten
}

func (c *LazyAssociatePacketConn) Upstream() any {
	return &c.AssociatePacketConn
}
