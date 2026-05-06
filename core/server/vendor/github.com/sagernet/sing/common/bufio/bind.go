package bufio

import (
	"net"

	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type BindPacketConn interface {
	N.NetPacketConn
	net.Conn
}

type bindPacketConn struct {
	N.NetPacketConn
	addr net.Addr
}

func NewBindPacketConn(conn net.PacketConn, addr net.Addr) BindPacketConn {
	return &bindPacketConn{
		NewPacketConn(conn),
		addr,
	}
}

func (c *bindPacketConn) Read(b []byte) (n int, err error) {
	n, _, err = c.ReadFrom(b)
	return
}

func (c *bindPacketConn) Write(b []byte) (n int, err error) {
	return c.WriteTo(b, c.addr)
}

func (c *bindPacketConn) CreateReadWaiter() (N.ReadWaiter, bool) {
	readWaiter, isReadWaiter := CreatePacketReadWaiter(c.NetPacketConn)
	if !isReadWaiter {
		return nil, false
	}
	return &bindPacketReadWaiter{readWaiter}, true
}

func (c *bindPacketConn) RemoteAddr() net.Addr {
	return c.addr
}

func (c *bindPacketConn) Upstream() any {
	return c.NetPacketConn
}

var (
	_ N.NetPacketConn         = (*UnbindPacketConn)(nil)
	_ N.PacketReadWaitCreator = (*UnbindPacketConn)(nil)
)

type UnbindPacketConn struct {
	N.ExtendedConn
	addr M.Socksaddr
}

func NewUnbindPacketConn(conn net.Conn) N.NetPacketConn {
	return &UnbindPacketConn{
		NewExtendedConn(conn),
		M.SocksaddrFromNet(conn.RemoteAddr()),
	}
}

func NewUnbindPacketConnWithAddr(conn net.Conn, addr M.Socksaddr) N.NetPacketConn {
	return &UnbindPacketConn{
		NewExtendedConn(conn),
		addr,
	}
}

func (c *UnbindPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	n, err = c.ExtendedConn.Read(p)
	if err == nil {
		addr = c.addr.UDPAddr()
	}
	return
}

func (c *UnbindPacketConn) WriteTo(p []byte, _ net.Addr) (n int, err error) {
	return c.ExtendedConn.Write(p)
}

func (c *UnbindPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	err = c.ExtendedConn.ReadBuffer(buffer)
	if err != nil {
		return
	}
	destination = c.addr
	return
}

func (c *UnbindPacketConn) WritePacket(buffer *buf.Buffer, _ M.Socksaddr) error {
	return c.ExtendedConn.WriteBuffer(buffer)
}

func (c *UnbindPacketConn) CreateReadWaiter() (N.PacketReadWaiter, bool) {
	readWaiter, isReadWaiter := CreateReadWaiter(c.ExtendedConn)
	if !isReadWaiter {
		return nil, false
	}
	return &unbindPacketReadWaiter{readWaiter, c.addr}, true
}

func (c *UnbindPacketConn) Upstream() any {
	return c.ExtendedConn
}

func NewServerPacketConn(conn net.PacketConn) N.ExtendedConn {
	return &serverPacketConn{
		NetPacketConn: NewPacketConn(conn),
	}
}

type serverPacketConn struct {
	N.NetPacketConn
	remoteAddr M.Socksaddr
}

func (c *serverPacketConn) Read(p []byte) (n int, err error) {
	n, addr, err := c.NetPacketConn.ReadFrom(p)
	if err != nil {
		return
	}
	c.remoteAddr = M.SocksaddrFromNet(addr)
	return
}

func (c *serverPacketConn) ReadBuffer(buffer *buf.Buffer) error {
	destination, err := c.NetPacketConn.ReadPacket(buffer)
	if err != nil {
		return err
	}
	c.remoteAddr = destination
	return nil
}

func (c *serverPacketConn) Write(p []byte) (n int, err error) {
	return c.NetPacketConn.WriteTo(p, c.remoteAddr.UDPAddr())
}

func (c *serverPacketConn) WriteBuffer(buffer *buf.Buffer) error {
	return c.NetPacketConn.WritePacket(buffer, c.remoteAddr)
}

func (c *serverPacketConn) RemoteAddr() net.Addr {
	return c.remoteAddr
}

func (c *serverPacketConn) Upstream() any {
	return c.NetPacketConn
}

func (c *serverPacketConn) CreateReadWaiter() (N.ReadWaiter, bool) {
	readWaiter, isReadWaiter := CreatePacketReadWaiter(c.NetPacketConn)
	if !isReadWaiter {
		return nil, false
	}
	return &serverPacketReadWaiter{c, readWaiter}, true
}
