package hysteria

import (
	"errors"
	"math/rand"
	"net"
	"os"
	"sync"
	"syscall"
	"time"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
)

const (
	packetQueueSize    = 1024
	udpBufferSize      = 2048
	defaultHopInterval = 30 * time.Second
)

type HopPacketConn struct {
	dialFunc        func(M.Socksaddr) (net.PacketConn, error)
	destination     M.Socksaddr
	ports           []uint16
	interval        time.Duration
	access          sync.Mutex
	prevConn        net.PacketConn
	currentConn     net.PacketConn
	portIndex       int
	readBufferSize  int
	writeBufferSize int
	packetChan      chan *buf.Buffer
	errChan         chan error
	doneChan        chan struct{}
	done            bool
}

func NewHopPacketConn(
	dialFunc func(M.Socksaddr) (net.PacketConn, error),
	destination M.Socksaddr,
	ports []uint16,
	interval time.Duration,
) (*HopPacketConn, error) {
	if interval == 0 {
		interval = defaultHopInterval
	}
	hopConn := &HopPacketConn{
		dialFunc:    dialFunc,
		destination: destination,
		ports:       ports,
		interval:    interval,
		packetChan:  make(chan *buf.Buffer, packetQueueSize),
		errChan:     make(chan error, 1),
		doneChan:    make(chan struct{}),
	}
	currentConn, err := dialFunc(hopConn.nextAddr())
	if err != nil {
		return nil, err
	}
	hopConn.currentConn = currentConn
	go hopConn.recvLoop(currentConn)
	go hopConn.hopLoop()
	return hopConn, nil
}

func (c *HopPacketConn) nextAddr() M.Socksaddr {
	c.portIndex = rand.Intn(len(c.ports))
	return M.Socksaddr{
		Addr: c.destination.Addr,
		Fqdn: c.destination.Fqdn,
		Port: c.ports[c.portIndex],
	}
}

func (c *HopPacketConn) recvLoop(conn net.PacketConn) {
	for {
		buffer := buf.NewSize(udpBufferSize)
		n, _, err := conn.ReadFrom(buffer.FreeBytes())
		if err != nil {
			buffer.Release()
			var netErr net.Error
			if errors.As(err, &netErr) && netErr.Timeout() {
				// Only pass through timeout errors here, not permanent errors
				// like connection closed. Connection close is normal as we close
				// the old connection to exit this loop every time we hop.
				c.errChan <- netErr
			}
			return
		}
		buffer.Truncate(n)
		select {
		case c.packetChan <- buffer:
		default:
			buffer.Release()
		}
	}
}

func (c *HopPacketConn) hopLoop() {
	ticker := time.NewTicker(c.interval)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			c.hop()
		case <-c.doneChan:
			return
		}
	}
}

func (c *HopPacketConn) hop() {
	c.access.Lock()
	defer c.access.Unlock()
	if c.done {
		return
	}
	nextAddr := c.nextAddr()
	newConn, err := c.dialFunc(nextAddr)
	if err != nil {
		return
	}
	if c.prevConn != nil {
		c.prevConn.Close()
	}
	c.prevConn = c.currentConn
	c.currentConn = newConn
	if c.readBufferSize > 0 {
		_ = trySetReadBuffer(newConn, c.readBufferSize)
	}
	if c.writeBufferSize > 0 {
		_ = trySetWriteBuffer(newConn, c.writeBufferSize)
	}
	go c.recvLoop(newConn)
}

func (c *HopPacketConn) ReadFrom(b []byte) (n int, addr net.Addr, err error) {
	for {
		select {
		case packet := <-c.packetChan:
			n = copy(b, packet.Bytes())
			packet.Release()
			return n, (*hopFakeAddr)(nil), nil
		case err = <-c.errChan:
			return 0, nil, err
		case <-c.doneChan:
			return 0, nil, net.ErrClosed
		}
	}
}

func (c *HopPacketConn) WriteTo(b []byte, _ net.Addr) (n int, err error) {
	c.access.Lock()
	defer c.access.Unlock()
	if c.done {
		return 0, net.ErrClosed
	}
	return c.currentConn.WriteTo(b, (*hopFakeAddr)(nil))
}

func (c *HopPacketConn) Close() error {
	c.access.Lock()
	defer c.access.Unlock()
	if c.done {
		return nil
	}
	if c.prevConn != nil {
		_ = c.prevConn.Close()
	}
	err := c.currentConn.Close()
	close(c.doneChan)
	c.done = true
	return err
}

func (c *HopPacketConn) LocalAddr() net.Addr {
	c.access.Lock()
	defer c.access.Unlock()
	return c.currentConn.LocalAddr()
}

func (c *HopPacketConn) SetDeadline(t time.Time) error {
	c.access.Lock()
	defer c.access.Unlock()
	if c.prevConn != nil {
		_ = c.prevConn.SetDeadline(t)
	}
	return c.currentConn.SetDeadline(t)
}

func (c *HopPacketConn) SetReadDeadline(t time.Time) error {
	c.access.Lock()
	defer c.access.Unlock()
	if c.prevConn != nil {
		_ = c.prevConn.SetReadDeadline(t)
	}
	return c.currentConn.SetReadDeadline(t)
}

func (c *HopPacketConn) SetWriteDeadline(t time.Time) error {
	c.access.Lock()
	defer c.access.Unlock()
	if c.prevConn != nil {
		_ = c.prevConn.SetWriteDeadline(t)
	}
	return c.currentConn.SetWriteDeadline(t)
}

func (c *HopPacketConn) SetReadBuffer(bytes int) error {
	c.access.Lock()
	defer c.access.Unlock()
	c.readBufferSize = bytes
	if c.prevConn != nil {
		_ = trySetReadBuffer(c.prevConn, bytes)
	}
	return trySetReadBuffer(c.currentConn, bytes)
}

func (c *HopPacketConn) SetWriteBuffer(bytes int) error {
	c.access.Lock()
	defer c.access.Unlock()
	c.writeBufferSize = bytes
	if c.prevConn != nil {
		_ = trySetWriteBuffer(c.prevConn, bytes)
	}
	return trySetWriteBuffer(c.currentConn, bytes)
}

func (c *HopPacketConn) SyscallConn() (syscall.RawConn, error) {
	c.access.Lock()
	defer c.access.Unlock()
	rawConn, isRawConn := common.Cast[syscall.Conn](c.currentConn)
	if !isRawConn {
		return nil, os.ErrInvalid
	}
	return rawConn.SyscallConn()
}

func trySetReadBuffer(pc any, bytes int) error {
	udpConn, isUDPConn := common.Cast[interface {
		SetReadBuffer(bytes int) error
	}](pc)
	if !isUDPConn {
		return nil
	}
	return udpConn.SetReadBuffer(bytes)
}

func trySetWriteBuffer(pc any, bytes int) error {
	udpConn, isUDPConn := common.Cast[interface {
		SetWriteBuffer(bytes int) error
	}](pc)
	if !isUDPConn {
		return nil
	}
	return udpConn.SetWriteBuffer(bytes)
}

type hopFakeAddr struct{}

func (a *hopFakeAddr) Network() string {
	return "udphop"
}

func (a *hopFakeAddr) String() string {
	return "<udphop>"
}
