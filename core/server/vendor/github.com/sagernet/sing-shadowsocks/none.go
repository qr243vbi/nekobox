package shadowsocks

import (
	"context"
	"net"
	"net/netip"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/udpnat"
)

const MethodNone = "none"

type NoneMethod struct{}

func NewNone() Method {
	return &NoneMethod{}
}

func (m *NoneMethod) Name() string {
	return MethodNone
}

func (m *NoneMethod) DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error) {
	shadowsocksConn := &noneConn{
		Conn:        conn,
		handshake:   true,
		destination: destination,
	}
	return shadowsocksConn, shadowsocksConn.clientHandshake()
}

func (m *NoneMethod) DialEarlyConn(conn net.Conn, destination M.Socksaddr) net.Conn {
	return &noneConn{
		Conn:        conn,
		destination: destination,
	}
}

func (m *NoneMethod) DialPacketConn(conn net.Conn) N.NetPacketConn {
	return &nonePacketConn{conn}
}

type noneConn struct {
	net.Conn

	handshake   bool
	destination M.Socksaddr
}

func (c *noneConn) clientHandshake() error {
	err := M.SocksaddrSerializer.WriteAddrPort(c.Conn, c.destination)
	if err != nil {
		return err
	}
	c.handshake = true
	return nil
}

func (c *noneConn) Write(b []byte) (n int, err error) {
	if c.handshake {
		return c.Conn.Write(b)
	}
	err = M.SocksaddrSerializer.WriteAddrPort(c.Conn, c.destination)
	if err != nil {
		return
	}
	c.handshake = true
	return c.Conn.Write(b)
}

func (c *noneConn) WriteBuffer(buffer *buf.Buffer) error {
	defer buffer.Release()
	if c.handshake {
		return common.Error(c.Conn.Write(buffer.Bytes()))
	}

	header := buf.With(buffer.ExtendHeader(M.SocksaddrSerializer.AddrPortLen(c.destination)))
	err := M.SocksaddrSerializer.WriteAddrPort(header, c.destination)
	if err != nil {
		return err
	}
	c.handshake = true
	return common.Error(c.Conn.Write(buffer.Bytes()))
}

func (c *noneConn) FrontHeadroom() int {
	if !c.handshake {
		return M.SocksaddrSerializer.AddrPortLen(c.destination)
	}
	return 0
}

func (c *noneConn) RemoteAddr() net.Addr {
	return c.destination.TCPAddr()
}

func (c *noneConn) Upstream() any {
	return c.Conn
}

func (c *noneConn) ReaderReplaceable() bool {
	return true
}

func (c *noneConn) WriterReplaceable() bool {
	return c.handshake
}

type nonePacketConn struct {
	net.Conn
}

func (c *nonePacketConn) ReadPacket(buffer *buf.Buffer) (M.Socksaddr, error) {
	_, err := buffer.ReadOnceFrom(c)
	if err != nil {
		return M.Socksaddr{}, err
	}
	return M.SocksaddrSerializer.ReadAddrPort(buffer)
}

func (c *nonePacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	defer buffer.Release()
	header := buf.With(buffer.ExtendHeader(M.SocksaddrSerializer.AddrPortLen(destination)))
	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		return err
	}
	return common.Error(buffer.WriteTo(c))
}

func (c *nonePacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	n, err = c.Read(p)
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
	_, err = buffer.Write(p)
	if err != nil {
		return
	}
	return len(p), nil
}

func (c *nonePacketConn) Headroom() int {
	return M.MaxSocksaddrLength
}

type NoneService struct {
	handler Handler
	udpNat  *udpnat.Service[netip.AddrPort]
}

func NewNoneService(udpTimeout int64, handler Handler) Service {
	s := &NoneService{
		handler: handler,
	}
	s.udpNat = udpnat.New[netip.AddrPort](udpTimeout, handler)
	return s
}

func (s *NoneService) Name() string {
	return MethodNone
}

func (s *NoneService) Password() string {
	return ""
}

func (s *NoneService) NewConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	destination, err := M.SocksaddrSerializer.ReadAddrPort(conn)
	if err != nil {
		return err
	}
	metadata.Protocol = "shadowsocks"
	metadata.Destination = destination
	return s.handler.NewConnection(ctx, conn, metadata)
}

func (s *NoneService) WriteIsThreadUnsafe() {
}

func (s *NoneService) NewPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	destination, err := M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		return err
	}
	metadata.Protocol = "shadowsocks"
	metadata.Destination = destination
	s.udpNat.NewPacket(ctx, metadata.Source.AddrPort(), buffer, metadata, func(natConn N.PacketConn) N.PacketWriter {
		return &nonePacketWriter{conn, natConn}
	})
	return nil
}

type nonePacketWriter struct {
	source N.PacketConn
	nat    N.PacketConn
}

func (w *nonePacketWriter) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	header := buf.With(buffer.ExtendHeader(M.SocksaddrSerializer.AddrPortLen(destination)))
	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		buffer.Release()
		return err
	}
	return w.source.WritePacket(buffer, M.SocksaddrFromNet(w.nat.LocalAddr()))
}

func (w *nonePacketWriter) Upstream() any {
	return w.source
}

func (w *nonePacketWriter) FrontHeadroom() int {
	return M.MaxSocksaddrLength
}

func (s *NoneService) NewError(ctx context.Context, err error) {
	s.handler.NewError(ctx, err)
}
