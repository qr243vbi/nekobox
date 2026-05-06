package hysteria

import (
	"bytes"
	"context"
	"encoding/binary"
	"errors"
	"io"
	"math"
	"net"
	"os"
	"sync"
	"sync/atomic"
	"time"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/cache"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/pipe"
)

var udpMessagePool = sync.Pool{
	New: func() interface{} {
		return new(udpMessage)
	},
}

func allocMessage() *udpMessage {
	message := udpMessagePool.Get().(*udpMessage)
	message.referenced = true
	return message
}

func releaseMessages(messages []*udpMessage) {
	for _, message := range messages {
		if message != nil {
			message.release()
		}
	}
}

type udpMessage struct {
	sessionID     uint32
	packetID      uint16
	fragmentID    uint8
	fragmentTotal uint8
	host          string
	port          uint16
	data          *buf.Buffer
	referenced    bool
}

func (m *udpMessage) release() {
	if !m.referenced {
		return
	}
	*m = udpMessage{}
	udpMessagePool.Put(m)
}

func (m *udpMessage) releaseMessage() {
	m.data.Release()
	m.release()
}

func (m *udpMessage) pack() *buf.Buffer {
	buffer := buf.NewSize(m.headerSize() + m.data.Len())
	common.Must(
		binary.Write(buffer, binary.BigEndian, m.sessionID),
		binary.Write(buffer, binary.BigEndian, uint16(len(m.host))),
		common.Error(buffer.WriteString(m.host)),
		binary.Write(buffer, binary.BigEndian, m.port),
		binary.Write(buffer, binary.BigEndian, m.packetID),
		binary.Write(buffer, binary.BigEndian, m.fragmentID),
		binary.Write(buffer, binary.BigEndian, m.fragmentTotal),
		binary.Write(buffer, binary.BigEndian, uint16(m.data.Len())),
		common.Error(buffer.Write(m.data.Bytes())),
	)
	return buffer
}

func (m *udpMessage) headerSize() int {
	return 14 + len(m.host)
}

func fragUDPMessage(message *udpMessage, maxPacketSize int) []*udpMessage {
	udpMTU := maxPacketSize - message.headerSize()
	if message.data.Len() <= udpMTU {
		return []*udpMessage{message}
	}
	var fragments []*udpMessage
	originPacket := message.data.Bytes()
	for remaining := len(originPacket); remaining > 0; remaining -= udpMTU {
		fragment := allocMessage()
		*fragment = *message
		if remaining > udpMTU {
			fragment.data = buf.As(originPacket[:udpMTU])
			originPacket = originPacket[udpMTU:]
		} else {
			fragment.data = buf.As(originPacket)
			originPacket = nil
		}
		fragments = append(fragments, fragment)
	}
	fragmentTotal := uint16(len(fragments))
	for index, fragment := range fragments {
		fragment.fragmentID = uint8(index)
		fragment.fragmentTotal = uint8(fragmentTotal)
		/*if index > 0 {
			fragment.destination = ""
			// not work in hysteria
		}*/
	}
	return fragments
}

type udpPacketConn struct {
	ctx             context.Context
	cancel          common.ContextCancelCauseFunc
	sessionID       uint32
	quicConn        *quic.Conn
	data            chan *udpMessage
	udpMTU          int
	packetId        atomic.Uint32
	closeOnce       sync.Once
	defragger       *udpDefragger
	onDestroy       func()
	readWaitOptions N.ReadWaitOptions
	readDeadline    pipe.Deadline
}

func newUDPPacketConn(ctx context.Context, quicConn *quic.Conn, onDestroy func()) *udpPacketConn {
	ctx, cancel := common.ContextWithCancelCause(ctx)
	return &udpPacketConn{
		ctx:          ctx,
		cancel:       cancel,
		quicConn:     quicConn,
		data:         make(chan *udpMessage, 64),
		udpMTU:       1200 - 3,
		defragger:    newUDPDefragger(),
		onDestroy:    onDestroy,
		readDeadline: pipe.MakeDeadline(),
	}
}

func (c *udpPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	select {
	case p := <-c.data:
		_, err = buffer.ReadOnceFrom(p.data)
		destination = M.ParseSocksaddrHostPort(p.host, p.port).Unwrap()
		p.releaseMessage()
		return
	case <-c.ctx.Done():
		return M.Socksaddr{}, io.ErrClosedPipe
	case <-c.readDeadline.Wait():
		return M.Socksaddr{}, os.ErrDeadlineExceeded
	}
}

func (c *udpPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	select {
	case pkt := <-c.data:
		n = copy(p, pkt.data.Bytes())
		destination := M.ParseSocksaddrHostPort(pkt.host, pkt.port).Unwrap()
		if destination.IsFqdn() {
			addr = destination
		} else {
			addr = destination.UDPAddr()
		}
		pkt.releaseMessage()
		return n, addr, nil
	case <-c.ctx.Done():
		return 0, nil, io.ErrClosedPipe
	case <-c.readDeadline.Wait():
		return 0, nil, os.ErrDeadlineExceeded
	}
}

func (c *udpPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	defer buffer.Release()
	select {
	case <-c.ctx.Done():
		return net.ErrClosed
	default:
	}
	if buffer.Len() > 0xffff {
		return &quic.DatagramTooLargeError{MaxDatagramPayloadSize: 0xffff}
	}
	packetId := uint16(c.packetId.Add(1) % math.MaxUint16)
	message := allocMessage()
	*message = udpMessage{
		sessionID:     c.sessionID,
		packetID:      packetId,
		fragmentTotal: 1,
		host:          destination.AddrString(),
		port:          destination.Port,
		data:          buffer,
	}
	defer message.releaseMessage()
	var err error
	if buffer.Len() > c.udpMTU-message.headerSize() {
		err = c.writePackets(fragUDPMessage(message, c.udpMTU))
	} else {
		err = c.writePacket(message)
	}
	if err == nil {
		return nil
	}
	var tooLargeErr *quic.DatagramTooLargeError
	if !errors.As(err, &tooLargeErr) {
		return err
	}
	return c.writePackets(fragUDPMessage(message, int(tooLargeErr.MaxDatagramPayloadSize-3)))
}

func (c *udpPacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	select {
	case <-c.ctx.Done():
		return 0, net.ErrClosed
	default:
	}
	if len(p) > 0xffff {
		return 0, &quic.DatagramTooLargeError{MaxDatagramPayloadSize: 0xffff}
	}
	packetId := uint16(c.packetId.Add(1) % math.MaxUint16)
	message := allocMessage()
	destination := M.SocksaddrFromNet(addr)
	*message = udpMessage{
		sessionID:     c.sessionID,
		packetID:      packetId,
		fragmentTotal: 1,
		host:          destination.AddrString(),
		port:          destination.Port,
		data:          buf.As(p),
	}
	if len(p) > c.udpMTU-message.headerSize() {
		err = c.writePackets(fragUDPMessage(message, c.udpMTU))
		if err == nil {
			return len(p), nil
		}
	} else {
		err = c.writePacket(message)
	}
	if err == nil {
		return len(p), nil
	}
	var tooLargeErr *quic.DatagramTooLargeError
	if !errors.As(err, &tooLargeErr) {
		return
	}
	err = c.writePackets(fragUDPMessage(message, int(tooLargeErr.MaxDatagramPayloadSize-3)))
	if err == nil {
		return len(p), nil
	}
	return
}

func (c *udpPacketConn) inputPacket(message *udpMessage) {
	if message.fragmentTotal <= 1 {
		select {
		case c.data <- message:
		default:
		}
	} else {
		newMessage := c.defragger.feed(message)
		if newMessage != nil {
			select {
			case c.data <- newMessage:
			default:
			}
		}
	}
}

func (c *udpPacketConn) writePackets(messages []*udpMessage) error {
	defer releaseMessages(messages)
	for _, message := range messages {
		err := c.writePacket(message)
		if err != nil {
			return err
		}
	}
	return nil
}

func (c *udpPacketConn) writePacket(message *udpMessage) error {
	buffer := message.pack()
	defer buffer.Release()
	return c.quicConn.SendDatagram(buffer.Bytes())
}

func (c *udpPacketConn) Close() error {
	c.closeWithError(os.ErrClosed)
	return nil
}

func (c *udpPacketConn) closeWithError(err error) {
	c.closeOnce.Do(func() {
		c.cancel(err)
		c.onDestroy()
	})
}

func (c *udpPacketConn) LocalAddr() net.Addr {
	return c.quicConn.LocalAddr()
}

func (c *udpPacketConn) SetDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *udpPacketConn) SetReadDeadline(t time.Time) error {
	c.readDeadline.Set(t)
	return nil
}

func (c *udpPacketConn) SetWriteDeadline(t time.Time) error {
	return os.ErrInvalid
}

type udpDefragger struct {
	packetMap *cache.LruCache[uint16, *packetItem]
}

func newUDPDefragger() *udpDefragger {
	return &udpDefragger{
		packetMap: cache.New(
			cache.WithAge[uint16, *packetItem](10),
			cache.WithUpdateAgeOnGet[uint16, *packetItem](),
			cache.WithEvict[uint16, *packetItem](func(key uint16, value *packetItem) {
				releaseMessages(value.messages)
			}),
		),
	}
}

type packetItem struct {
	access   sync.Mutex
	messages []*udpMessage
	count    uint8
}

func (d *udpDefragger) feed(m *udpMessage) *udpMessage {
	if m.fragmentTotal <= 1 {
		return m
	}
	if m.fragmentID >= m.fragmentTotal {
		return nil
	}
	item, _ := d.packetMap.LoadOrStore(m.packetID, newPacketItem)
	item.access.Lock()
	defer item.access.Unlock()
	if int(m.fragmentTotal) != len(item.messages) {
		releaseMessages(item.messages)
		item.messages = make([]*udpMessage, m.fragmentTotal)
		item.count = 1
		item.messages[m.fragmentID] = m
		return nil
	}
	if item.messages[m.fragmentID] != nil {
		return nil
	}
	item.messages[m.fragmentID] = m
	item.count++
	if int(item.count) != len(item.messages) {
		return nil
	}
	newMessage := allocMessage()
	newMessage.sessionID = m.sessionID
	newMessage.packetID = m.packetID
	newMessage.host = item.messages[0].host
	newMessage.port = item.messages[0].port
	var finalLength int
	for _, message := range item.messages {
		finalLength += message.data.Len()
	}
	if finalLength > 0 {
		newMessage.data = buf.NewSize(finalLength)
		for _, message := range item.messages {
			newMessage.data.Write(message.data.Bytes())
			message.releaseMessage()
		}
		item.messages = nil
		return newMessage
	} else {
		newMessage.release()
		for _, message := range item.messages {
			message.releaseMessage()
		}
	}
	item.messages = nil
	return nil
}

func newPacketItem() *packetItem {
	return new(packetItem)
}

func decodeUDPMessage(message *udpMessage, data []byte) error {
	reader := bytes.NewReader(data)
	err := binary.Read(reader, binary.BigEndian, &message.sessionID)
	if err != nil {
		return err
	}
	var hostLen uint16
	err = binary.Read(reader, binary.BigEndian, &hostLen)
	if err != nil {
		return err
	}
	hostBytes := make([]byte, hostLen)
	_, err = io.ReadFull(reader, hostBytes)
	if err != nil {
		return err
	}
	message.host = string(hostBytes)
	err = binary.Read(reader, binary.BigEndian, &message.port)
	if err != nil {
		return err
	}
	err = binary.Read(reader, binary.BigEndian, &message.packetID)
	if err != nil {
		return err
	}
	err = binary.Read(reader, binary.BigEndian, &message.fragmentID)
	if err != nil {
		return err
	}
	err = binary.Read(reader, binary.BigEndian, &message.fragmentTotal)
	if err != nil {
		return err
	}
	var dataLen uint16
	err = binary.Read(reader, binary.BigEndian, &dataLen)
	if err != nil {
		return err
	}
	if reader.Len() != int(dataLen) {
		return E.New("invalid data length")
	}
	message.data = buf.As(data[len(data)-reader.Len():])
	return nil
}
