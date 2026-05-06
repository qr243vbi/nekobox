package bufio

import (
	"net"
	"net/netip"

	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type NATPacketConn interface {
	N.NetPacketConn
	UpdateDestination(destinationAddress netip.Addr)
}

func NewUnidirectionalNATPacketConn(conn N.NetPacketConn, origin M.Socksaddr, destination M.Socksaddr) NATPacketConn {
	return &unidirectionalNATPacketConn{
		NetPacketConn: conn,
		origin:        socksaddrWithoutPort(origin),
		destination:   socksaddrWithoutPort(destination),
	}
}

func NewNATPacketConn(conn N.NetPacketConn, origin M.Socksaddr, destination M.Socksaddr) NATPacketConn {
	return &bidirectionalNATPacketConn{
		NetPacketConn: conn,
		origin:        socksaddrWithoutPort(origin),
		destination:   socksaddrWithoutPort(destination),
	}
}

func NewDestinationNATPacketConn(conn N.NetPacketConn, origin M.Socksaddr, destination M.Socksaddr) NATPacketConn {
	return &destinationNATPacketConn{
		NetPacketConn: conn,
		origin:        origin,
		destination:   destination,
	}
}

type unidirectionalNATPacketConn struct {
	N.NetPacketConn
	origin      M.Socksaddr
	destination M.Socksaddr
}

func (c *unidirectionalNATPacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	destination := M.SocksaddrFromNet(addr)
	if socksaddrWithoutPort(destination) == c.destination {
		destination = M.Socksaddr{
			Addr: c.origin.Addr,
			Fqdn: c.origin.Fqdn,
			Port: destination.Port,
		}
	}
	return c.NetPacketConn.WriteTo(p, destination.UDPAddr())
}

func (c *unidirectionalNATPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	if socksaddrWithoutPort(destination) == c.destination {
		destination = M.Socksaddr{
			Addr: c.origin.Addr,
			Fqdn: c.origin.Fqdn,
			Port: destination.Port,
		}
	}
	return c.NetPacketConn.WritePacket(buffer, destination)
}

func (c *unidirectionalNATPacketConn) UpdateDestination(destinationAddress netip.Addr) {
	c.destination = M.SocksaddrFrom(destinationAddress, c.destination.Port)
}

func (c *unidirectionalNATPacketConn) RemoteAddr() net.Addr {
	return c.destination.UDPAddr()
}

func (c *unidirectionalNATPacketConn) Upstream() any {
	return c.NetPacketConn
}

type bidirectionalNATPacketConn struct {
	N.NetPacketConn
	origin      M.Socksaddr
	destination M.Socksaddr
}

func (c *bidirectionalNATPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	n, addr, err = c.NetPacketConn.ReadFrom(p)
	if err != nil {
		return
	}
	destination := M.SocksaddrFromNet(addr)
	if socksaddrWithoutPort(destination) == c.origin {
		destination = M.Socksaddr{
			Addr: c.destination.Addr,
			Fqdn: c.destination.Fqdn,
			Port: destination.Port,
		}
	}
	addr = destination.UDPAddr()
	return
}

func (c *bidirectionalNATPacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	destination := M.SocksaddrFromNet(addr)
	if socksaddrWithoutPort(destination) == c.destination {
		destination = M.Socksaddr{
			Addr: c.origin.Addr,
			Fqdn: c.origin.Fqdn,
			Port: destination.Port,
		}
	}
	return c.NetPacketConn.WriteTo(p, destination.UDPAddr())
}

func (c *bidirectionalNATPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	destination, err = c.NetPacketConn.ReadPacket(buffer)
	if err != nil {
		return
	}
	if socksaddrWithoutPort(destination) == c.origin {
		destination = M.Socksaddr{
			Addr: c.destination.Addr,
			Fqdn: c.destination.Fqdn,
			Port: destination.Port,
		}
	}
	return
}

func (c *bidirectionalNATPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	if socksaddrWithoutPort(destination) == c.destination {
		destination = M.Socksaddr{
			Addr: c.origin.Addr,
			Fqdn: c.origin.Fqdn,
			Port: destination.Port,
		}
	}
	return c.NetPacketConn.WritePacket(buffer, destination)
}

func (c *bidirectionalNATPacketConn) UpdateDestination(destinationAddress netip.Addr) {
	c.destination = M.SocksaddrFrom(destinationAddress, c.destination.Port)
}

func (c *bidirectionalNATPacketConn) Upstream() any {
	return c.NetPacketConn
}

func (c *bidirectionalNATPacketConn) RemoteAddr() net.Addr {
	return c.destination.UDPAddr()
}

type destinationNATPacketConn struct {
	N.NetPacketConn
	origin      M.Socksaddr
	destination M.Socksaddr
}

func (c *destinationNATPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	n, addr, err = c.NetPacketConn.ReadFrom(p)
	if err != nil {
		return
	}
	if M.SocksaddrFromNet(addr) == c.origin {
		addr = c.destination.UDPAddr()
	}
	return
}

func (c *destinationNATPacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	if M.SocksaddrFromNet(addr) == c.destination {
		addr = c.origin.UDPAddr()
	}
	return c.NetPacketConn.WriteTo(p, addr)
}

func (c *destinationNATPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	destination, err = c.NetPacketConn.ReadPacket(buffer)
	if err != nil {
		return
	}
	if destination == c.origin {
		destination = c.destination
	}
	return
}

func (c *destinationNATPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	if destination == c.destination {
		destination = c.origin
	}
	return c.NetPacketConn.WritePacket(buffer, destination)
}

func (c *destinationNATPacketConn) UpdateDestination(destinationAddress netip.Addr) {
	c.destination = M.SocksaddrFrom(destinationAddress, c.destination.Port)
}

func (c *destinationNATPacketConn) Upstream() any {
	return c.NetPacketConn
}

func (c *destinationNATPacketConn) RemoteAddr() net.Addr {
	return c.destination.UDPAddr()
}

func socksaddrWithoutPort(destination M.Socksaddr) M.Socksaddr {
	destination.Port = 0
	return destination
}
