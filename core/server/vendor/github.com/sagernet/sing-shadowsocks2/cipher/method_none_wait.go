package cipher

import (
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

var _ N.PacketReadWaitCreator = (*nonePacketConn)(nil)

func (c *nonePacketConn) CreateReadWaiter() (N.PacketReadWaiter, bool) {
	readWaiter, isReadWaiter := bufio.CreateReadWaiter(c.conn)
	if !isReadWaiter {
		return nil, false
	}
	return &nonePacketReadWaiter{readWaiter}, true
}

var _ N.PacketReadWaiter = (*nonePacketReadWaiter)(nil)

type nonePacketReadWaiter struct {
	readWaiter N.ReadWaiter
}

func (w *nonePacketReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	return w.readWaiter.InitializeReadWaiter(options)
}

func (w *nonePacketReadWaiter) WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	buffer, err = w.readWaiter.WaitReadBuffer()
	if err != nil {
		return
	}
	destination, err = M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		buffer.Release()
		return nil, M.Socksaddr{}, err
	}
	return
}
