package bufio

import (
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

var _ N.ReadWaiter = (*bindPacketReadWaiter)(nil)

type bindPacketReadWaiter struct {
	readWaiter N.PacketReadWaiter
}

func (w *bindPacketReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	return w.readWaiter.InitializeReadWaiter(options)
}

func (w *bindPacketReadWaiter) WaitReadBuffer() (buffer *buf.Buffer, err error) {
	buffer, _, err = w.readWaiter.WaitReadPacket()
	return
}

var _ N.PacketReadWaiter = (*unbindPacketReadWaiter)(nil)

type unbindPacketReadWaiter struct {
	readWaiter N.ReadWaiter
	addr       M.Socksaddr
}

func (w *unbindPacketReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	return w.readWaiter.InitializeReadWaiter(options)
}

func (w *unbindPacketReadWaiter) WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	buffer, err = w.readWaiter.WaitReadBuffer()
	if err != nil {
		return
	}
	destination = w.addr
	return
}

var _ N.ReadWaiter = (*serverPacketReadWaiter)(nil)

type serverPacketReadWaiter struct {
	*serverPacketConn
	readWaiter N.PacketReadWaiter
}

func (w *serverPacketReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	return w.readWaiter.InitializeReadWaiter(options)
}

func (w *serverPacketReadWaiter) WaitReadBuffer() (buffer *buf.Buffer, err error) {
	buffer, destination, err := w.readWaiter.WaitReadPacket()
	if err != nil {
		return
	}
	w.remoteAddr = destination
	return
}
