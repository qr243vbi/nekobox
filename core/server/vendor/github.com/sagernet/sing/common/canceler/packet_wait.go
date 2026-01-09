package canceler

import (
	"time"

	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type timerPackerReadWaiter struct {
	*TimerPacketConn
	readWaiter N.PacketReadWaiter
}

func (c *TimerPacketConn) CreateReadWaiter() (N.PacketReadWaiter, bool) {
	readWaiter, isReadWaiter := bufio.CreatePacketReadWaiter(c.PacketConn)
	if !isReadWaiter {
		return nil, false
	}
	return &timerPackerReadWaiter{c, readWaiter}, true
}

func (w *timerPackerReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	return w.readWaiter.InitializeReadWaiter(options)
}

func (w *timerPackerReadWaiter) WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	buffer, destination, err = w.readWaiter.WaitReadPacket()
	if err == nil {
		w.instance.Update()
	}
	return
}

type timeoutPacketReadWaiter struct {
	*TimeoutPacketConn
	readWaiter N.PacketReadWaiter
}

func (c *TimeoutPacketConn) CreateReadWaiter() (N.PacketReadWaiter, bool) {
	readWaiter, isReadWaiter := bufio.CreatePacketReadWaiter(c.PacketConn)
	if !isReadWaiter {
		return nil, false
	}
	return &timeoutPacketReadWaiter{c, readWaiter}, true
}

func (w *timeoutPacketReadWaiter) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	return w.readWaiter.InitializeReadWaiter(options)
}

func (w *timeoutPacketReadWaiter) WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	for {
		err = w.PacketConn.SetReadDeadline(time.Now().Add(w.timeout))
		if err != nil {
			return
		}
		buffer, destination, err = w.readWaiter.WaitReadPacket()
		if err == nil {
			w.active = time.Now()
			return
		} else if E.IsTimeout(err) {
			if time.Since(w.active) > w.timeout {
				w.cancel(err)
				return
			}
		} else {
			return
		}
	}
}
