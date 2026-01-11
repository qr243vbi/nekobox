package shadowaead

import (
	"context"
	"crypto/rand"
	"io"
	"net"
	"net/netip"
	"sync"

	"github.com/sagernet/sing-shadowsocks"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/rw"
	"github.com/sagernet/sing/common/udpnat"
)

var ErrBadHeader = E.New("bad header")

var _ shadowsocks.Service = (*Service)(nil)

type Service struct {
	*Method
	password string
	handler  shadowsocks.Handler
	udpNat   *udpnat.Service[netip.AddrPort]
}

func NewService(method string, key []byte, password string, udpTimeout int64, handler shadowsocks.Handler) (*Service, error) {
	m, err := New(method, key, password)
	if err != nil {
		return nil, err
	}
	s := &Service{
		Method:  m,
		handler: handler,
		udpNat:  udpnat.New[netip.AddrPort](udpTimeout, handler),
	}
	return s, nil
}

func (s *Service) Name() string {
	return s.name
}

func (s *Service) Password() string {
	return s.password
}

func (s *Service) NewConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	err := s.newConnection(ctx, conn, metadata)
	if err != nil {
		err = &shadowsocks.ServerConnError{Conn: conn, Source: metadata.Source, Cause: err}
	}
	return err
}

func (s *Service) newConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	header := buf.NewSize(s.keySaltLength + PacketLengthBufferSize + Overhead)
	defer header.Release()

	_, err := header.ReadFullFrom(conn, header.FreeLen())
	if err != nil {
		return E.Cause(err, "read header")
	} else if !header.IsFull() {
		return ErrBadHeader
	}

	key := buf.NewSize(s.keySaltLength)
	Kdf(s.key, header.To(s.keySaltLength), key)
	readCipher, err := s.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return err
	}
	reader := NewReader(conn, readCipher, MaxPacketSize)

	err = reader.ReadWithLengthChunk(header.From(s.keySaltLength))
	if err != nil {
		return err
	}

	destination, err := M.SocksaddrSerializer.ReadAddrPort(reader)
	if err != nil {
		return err
	}

	metadata.Protocol = "shadowsocks"
	metadata.Destination = destination

	return s.handler.NewConnection(ctx, &serverConn{
		Method: s.Method,
		Conn:   conn,
		reader: reader,
	}, metadata)
}

func (s *Service) NewError(ctx context.Context, err error) {
	s.handler.NewError(ctx, err)
}

type serverConn struct {
	*Method
	net.Conn
	access sync.Mutex
	reader *Reader
	writer *Writer
}

func (c *serverConn) writeResponse(payload []byte) (n int, err error) {
	salt := buf.NewSize(c.keySaltLength)
	salt.WriteRandom(c.keySaltLength)

	key := buf.NewSize(c.keySaltLength)

	Kdf(c.key, salt.Bytes(), key)
	writeCipher, err := c.constructor(key.Bytes())
	key.Release()
	if err != nil {
		salt.Release()
		return
	}
	writer := NewWriter(c.Conn, writeCipher, MaxPacketSize)

	header := writer.Buffer()
	common.Must1(header.Write(salt.Bytes()))
	salt.Release()

	bufferedWriter := writer.BufferedWriter(header.Len())
	if len(payload) > 0 {
		n, err = bufferedWriter.Write(payload)
		if err != nil {
			return
		}
	}

	err = bufferedWriter.Flush()
	if err != nil {
		return
	}

	c.writer = writer
	return
}

func (c *serverConn) Read(b []byte) (n int, err error) {
	return c.reader.Read(b)
}

func (c *serverConn) Write(p []byte) (n int, err error) {
	if c.writer != nil {
		return c.writer.Write(p)
	}
	c.access.Lock()
	if c.writer != nil {
		c.access.Unlock()
		return c.writer.Write(p)
	}
	defer c.access.Unlock()
	return c.writeResponse(p)
}

func (c *serverConn) WriteTo(w io.Writer) (n int64, err error) {
	return c.reader.WriteTo(w)
}

func (c *serverConn) NeedAdditionalReadDeadline() bool {
	return true
}

func (c *serverConn) Upstream() any {
	return c.Conn
}

func (c *serverConn) ReaderMTU() int {
	return MaxPacketSize
}

func (c *Service) WriteIsThreadUnsafe() {
}

func (s *Service) NewPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	err := s.newPacket(ctx, conn, buffer, metadata)
	if err != nil {
		err = &shadowsocks.ServerPacketError{Source: metadata.Source, Cause: err}
	}
	return err
}

func (s *Service) newPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	if buffer.Len() < s.keySaltLength {
		return io.ErrShortBuffer
	}
	key := buf.NewSize(s.keySaltLength)
	Kdf(s.key, buffer.To(s.keySaltLength), key)
	readCipher, err := s.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return err
	}
	packet, err := readCipher.Open(buffer.Index(s.keySaltLength), rw.ZeroBytes[:readCipher.NonceSize()], buffer.From(s.keySaltLength), nil)
	if err != nil {
		return err
	}
	buffer.Advance(s.keySaltLength)
	buffer.Truncate(len(packet))

	destination, err := M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		return err
	}

	metadata.Protocol = "shadowsocks"
	metadata.Destination = destination
	s.udpNat.NewPacket(ctx, metadata.Source.AddrPort(), buffer, metadata, func(natConn N.PacketConn) N.PacketWriter {
		return &serverPacketWriter{s.Method, conn, natConn}
	})
	return nil
}

type serverPacketWriter struct {
	*Method
	source N.PacketConn
	nat    N.PacketConn
}

func (w *serverPacketWriter) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	header := buffer.ExtendHeader(w.keySaltLength + M.SocksaddrSerializer.AddrPortLen(destination))
	common.Must1(io.ReadFull(rand.Reader, header[:w.keySaltLength]))
	err := M.SocksaddrSerializer.WriteAddrPort(buf.With(header[w.keySaltLength:]), destination)
	if err != nil {
		buffer.Release()
		return err
	}
	key := buf.NewSize(w.keySaltLength)
	Kdf(w.key, buffer.To(w.keySaltLength), key)
	writeCipher, err := w.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return err
	}
	writeCipher.Seal(buffer.From(w.keySaltLength)[:0], rw.ZeroBytes[:writeCipher.NonceSize()], buffer.From(w.keySaltLength), nil)
	buffer.Extend(Overhead)
	return w.source.WritePacket(buffer, M.SocksaddrFromNet(w.nat.LocalAddr()))
}

func (w *serverPacketWriter) FrontHeadroom() int {
	return w.keySaltLength + M.MaxSocksaddrLength
}

func (w *serverPacketWriter) RearHeadroom() int {
	return Overhead
}

func (w *serverPacketWriter) WriterMTU() int {
	return MaxPacketSize
}

func (w *serverPacketWriter) Upstream() any {
	return w.source
}

func (w *serverPacketWriter) ReaderMTU() int {
	return MaxPacketSize
}

func (w *serverPacketWriter) WriteIsThreadUnsafe() {
}
