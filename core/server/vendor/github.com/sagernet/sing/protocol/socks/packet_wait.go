package socks

import (
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

var _ N.PacketReadWaitCreator = (*AssociatePacketConn)(nil)

func (c *AssociatePacketConn) CreateReadWaiter() (N.PacketReadWaiter, bool) {
	readWaiter, isReadWaiter := bufio.CreateReadWaiter(c.conn)
	if !isReadWaiter {
		return nil, false
	}
	return &AssociatePacketReadWaiter{c, readWaiter}, true
}

var _ N.PacketReadWaiter = (*AssociatePacketReadWaiter)(nil)

type AssociatePacketReadWaiter struct {
	conn       *AssociatePacketConn
	readWaiter N.ReadWaiter
}

func (w *AssociatePacketReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	return w.readWaiter.InitializeReadWaiter(options)
}

func (w *AssociatePacketReadWaiter) WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	buffer, err = w.readWaiter.WaitReadBuffer()
	if err != nil {
		return
	}
	if buffer.Len() < 3 {
		buffer.Release()
		return nil, M.Socksaddr{}, ErrInvalidPacket
	}
	buffer.Advance(3)
	destination, err = M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		buffer.Release()
		return
	}
	w.conn.remoteAddr = destination
	return
}
