package canceler

import (
	"context"
	"time"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

type PacketConn interface {
	N.PacketConn
	Timeout() time.Duration
	SetTimeout(timeout time.Duration) bool
}

type TimerPacketConn struct {
	N.PacketConn
	instance *Instance
}

func NewPacketConn(ctx context.Context, conn N.PacketConn, timeout time.Duration) (context.Context, N.PacketConn) {
	if timeoutConn, isTimeoutConn := common.Cast[PacketConn](conn); isTimeoutConn {
		oldTimeout := timeoutConn.Timeout()
		if oldTimeout > 0 && timeout >= oldTimeout {
			return ctx, conn
		}
		if timeoutConn.SetTimeout(timeout) {
			return ctx, conn
		}
	}
	err := conn.SetReadDeadline(time.Time{})
	if err == nil {
		return NewTimeoutPacketConn(ctx, conn, timeout)
	}
	ctx, cancel := common.ContextWithCancelCause(ctx)
	instance := New(ctx, cancel, timeout)
	return ctx, &TimerPacketConn{conn, instance}
}

func (c *TimerPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	destination, err = c.PacketConn.ReadPacket(buffer)
	if err == nil {
		c.instance.Update()
	}
	return
}

func (c *TimerPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	err := c.PacketConn.WritePacket(buffer, destination)
	if err == nil {
		c.instance.Update()
	}
	return err
}

func (c *TimerPacketConn) Timeout() time.Duration {
	return c.instance.Timeout()
}

func (c *TimerPacketConn) SetTimeout(timeout time.Duration) bool {
	return c.instance.SetTimeout(timeout)
}

func (c *TimerPacketConn) Close() error {
	return common.Close(
		c.PacketConn,
		c.instance,
	)
}

func (c *TimerPacketConn) Upstream() any {
	return c.PacketConn
}
