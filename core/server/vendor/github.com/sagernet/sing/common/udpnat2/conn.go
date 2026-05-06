package udpnat

import (
	"io"
	"net"
	"net/netip"
	"os"
	"sync"
	"time"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/canceler"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/pipe"
	"github.com/sagernet/sing/contrab/freelru"
)

type Conn interface {
	N.PacketConn
	SetHandler(handler N.UDPHandlerEx)
	canceler.PacketConn
}

var _ Conn = (*natConn)(nil)

type natConn struct {
	cache           freelru.Cache[netip.AddrPort, *natConn]
	writer          N.PacketWriter
	localAddr       M.Socksaddr
	handlerAccess   sync.RWMutex
	handler         N.UDPHandlerEx
	packetChan      chan *N.PacketBuffer
	closeOnce       sync.Once
	doneChan        chan struct{}
	readDeadline    pipe.Deadline
	readWaitOptions N.ReadWaitOptions
}

func (c *natConn) ReadPacket(buffer *buf.Buffer) (addr M.Socksaddr, err error) {
	select {
	case p := <-c.packetChan:
		_, err = buffer.ReadOnceFrom(p.Buffer)
		destination := p.Destination
		p.Buffer.Release()
		N.PutPacketBuffer(p)
		return destination, err
	case <-c.doneChan:
		return M.Socksaddr{}, io.ErrClosedPipe
	case <-c.readDeadline.Wait():
		return M.Socksaddr{}, os.ErrDeadlineExceeded
	}
}

func (c *natConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	return c.writer.WritePacket(buffer, destination)
}

func (c *natConn) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	c.handlerAccess.Lock()
	defer c.handlerAccess.Unlock()
	c.readWaitOptions = options
	return false
}

func (c *natConn) WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error) {
	select {
	case packet := <-c.packetChan:
		buffer = c.readWaitOptions.Copy(packet.Buffer)
		destination = packet.Destination
		N.PutPacketBuffer(packet)
		return
	case <-c.doneChan:
		return nil, M.Socksaddr{}, io.ErrClosedPipe
	case <-c.readDeadline.Wait():
		return nil, M.Socksaddr{}, os.ErrDeadlineExceeded
	}
}

func (c *natConn) SetHandler(handler N.UDPHandlerEx) {
	c.handlerAccess.Lock()
	c.handler = handler
	c.readWaitOptions = N.NewReadWaitOptions(c.writer, handler)
	c.handlerAccess.Unlock()
fetch:
	for {
		select {
		case packet := <-c.packetChan:
			c.handler.NewPacketEx(packet.Buffer, packet.Destination)
			N.PutPacketBuffer(packet)
			continue fetch
		default:
			break fetch
		}
	}
}

func (c *natConn) Timeout() time.Duration {
	rawConn, lifetime, loaded := c.cache.PeekWithLifetime(c.localAddr.AddrPort())
	if !loaded || rawConn != c {
		return 0
	}
	return time.Until(lifetime)
}

func (c *natConn) SetTimeout(timeout time.Duration) bool {
	return c.cache.UpdateLifetime(c.localAddr.AddrPort(), c, timeout)
}

func (c *natConn) Close() error {
	c.closeOnce.Do(func() {
		close(c.doneChan)
		common.Close(c.handler)
	})
	return nil
}

func (c *natConn) LocalAddr() net.Addr {
	return c.localAddr
}

func (c *natConn) RemoteAddr() net.Addr {
	return M.Socksaddr{}
}

func (c *natConn) SetDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *natConn) SetReadDeadline(t time.Time) error {
	c.readDeadline.Set(t)
	return nil
}

func (c *natConn) SetWriteDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *natConn) Upstream() any {
	return c.writer
}
