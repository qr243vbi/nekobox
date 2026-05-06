package cipher

import (
	"context"
	"net"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

const MethodNone = "none"

func init() {
	RegisterMethod([]string{MethodNone}, func(ctx context.Context, method string, options MethodOptions) (Method, error) {
		return &noneMethod{}, nil
	})
}

var _ Method = (*noneMethod)(nil)

type noneMethod struct{}

func (m *noneMethod) DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error) {
	err := M.SocksaddrSerializer.WriteAddrPort(conn, destination)
	if err != nil {
		return nil, err
	}
	return conn, nil
}

func (m *noneMethod) DialEarlyConn(conn net.Conn, destination M.Socksaddr) net.Conn {
	return &noneConn{
		ExtendedConn: bufio.NewExtendedConn(conn),
		destination:  destination,
	}
}

func (m *noneMethod) DialPacketConn(conn net.Conn) N.NetPacketConn {
	extendedConn := bufio.NewExtendedConn(conn)
	return &nonePacketConn{extendedConn, extendedConn}
}

var (
	_ N.ExtendedConn       = (*noneConn)(nil)
	_ N.FrontHeadroom      = (*noneConn)(nil)
	_ N.ReaderWithUpstream = (*noneConn)(nil)
	_ N.WriterWithUpstream = (*noneConn)(nil)
	_ common.WithUpstream  = (*noneConn)(nil)
)

type noneConn struct {
	N.ExtendedConn
	destination    M.Socksaddr
	requestWritten bool
}

func (c *noneConn) Write(p []byte) (n int, err error) {
	if !c.requestWritten {
		buffer := buf.NewSize(M.SocksaddrSerializer.AddrPortLen(c.destination) + len(p))
		defer buffer.Release()
		err = M.SocksaddrSerializer.WriteAddrPort(buffer, c.destination)
		if err != nil {
			return
		}
		common.Must1(buffer.Write(p))
		_, err = c.ExtendedConn.Write(buffer.Bytes())
		if err != nil {
			return
		}
		c.requestWritten = true
		n = len(p)
		return
	}
	return c.ExtendedConn.Write(p)
}

func (c *noneConn) WriteBuffer(buffer *buf.Buffer) error {
	if !c.requestWritten {
		header := buf.With(buffer.ExtendHeader(M.SocksaddrSerializer.AddrPortLen(c.destination)))
		err := M.SocksaddrSerializer.WriteAddrPort(header, c.destination)
		if err != nil {
			return err
		}
		c.requestWritten = true
	}
	return c.ExtendedConn.WriteBuffer(buffer)
}

func (c *noneConn) FrontHeadroom() int {
	return M.MaxSocksaddrLength
}

func (c *noneConn) ReaderReplaceable() bool {
	return true
}

func (c *noneConn) WriterReplaceable() bool {
	return c.requestWritten
}

func (c *noneConn) Upstream() any {
	return c.ExtendedConn
}

var (
	_ N.NetPacketConn     = (*nonePacketConn)(nil)
	_ N.FrontHeadroom     = (*nonePacketConn)(nil)
	_ common.WithUpstream = (*nonePacketConn)(nil)
)

type nonePacketConn struct {
	N.AbstractConn
	conn N.ExtendedConn
}

func (c *nonePacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	n, err = c.conn.Read(p)
	if err != nil {
		return
	}
	buffer := buf.As(p[:n])
	destination, err := M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		return
	}
	if destination.IsFqdn() {
		addr = destination
	} else {
		addr = destination.UDPAddr()
	}
	n = copy(p, buffer.Bytes())
	return
}

func (c *nonePacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	destination := M.SocksaddrFromNet(addr)
	buffer := buf.NewSize(M.SocksaddrSerializer.AddrPortLen(destination) + len(p))
	defer buffer.Release()
	err = M.SocksaddrSerializer.WriteAddrPort(buffer, destination)
	if err != nil {
		return
	}
	common.Must1(buffer.Write(p))
	_, err = c.conn.Write(buffer.Bytes())
	if err != nil {
		return
	}
	n = len(p)
	return
}

func (c *nonePacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	err = c.conn.ReadBuffer(buffer)
	if err != nil {
		return
	}
	return M.SocksaddrSerializer.ReadAddrPort(buffer)
}

func (c *nonePacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	header := buf.With(buffer.ExtendHeader(M.SocksaddrSerializer.AddrPortLen(destination)))
	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		return err
	}
	return c.conn.WriteBuffer(buffer)
}

func (c *nonePacketConn) FrontHeadroom() int {
	return M.MaxSocksaddrLength
}

func (c *nonePacketConn) Upstream() any {
	return c.conn
}
