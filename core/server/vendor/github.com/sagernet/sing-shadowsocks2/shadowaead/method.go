package shadowaead

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"net"

	C "github.com/sagernet/sing-shadowsocks2/cipher"
	"github.com/sagernet/sing-shadowsocks2/internal/legacykey"
	"github.com/sagernet/sing-shadowsocks2/internal/shadowio"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/rw"

	"golang.org/x/crypto/chacha20poly1305"
)

var MethodList = []string{
	"aes-128-gcm",
	"aes-192-gcm",
	"aes-256-gcm",
	"chacha20-ietf-poly1305",
	"xchacha20-ietf-poly1305",
}

func init() {
	C.RegisterMethod(MethodList, func(ctx context.Context, methodName string, options C.MethodOptions) (C.Method, error) {
		return NewMethod(ctx, methodName, options)
	})
}

type Method struct {
	keySaltLength int
	constructor   func(key []byte) (cipher.AEAD, error)
	key           []byte
}

func NewMethod(ctx context.Context, methodName string, options C.MethodOptions) (*Method, error) {
	m := &Method{}
	switch methodName {
	case "aes-128-gcm":
		m.keySaltLength = 16
		m.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
	case "aes-192-gcm":
		m.keySaltLength = 24
		m.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
	case "aes-256-gcm":
		m.keySaltLength = 32
		m.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
	case "chacha20-ietf-poly1305":
		m.keySaltLength = 32
		m.constructor = chacha20poly1305.New
	case "xchacha20-ietf-poly1305":
		m.keySaltLength = 32
		m.constructor = chacha20poly1305.NewX
	}
	if len(options.Key) == m.keySaltLength {
		m.key = options.Key
	} else if len(options.Key) > 0 {
		return nil, E.New("bad key length, required ", m.keySaltLength, ", got ", len(options.Key))
	} else if options.Password == "" {
		return nil, C.ErrMissingPassword
	} else {
		m.key = legacykey.Key([]byte(options.Password), m.keySaltLength)
	}
	return m, nil
}

func aeadCipher(block func(key []byte) (cipher.Block, error), aead func(block cipher.Block) (cipher.AEAD, error)) func(key []byte) (cipher.AEAD, error) {
	return func(key []byte) (cipher.AEAD, error) {
		b, err := block(key)
		if err != nil {
			return nil, err
		}
		return aead(b)
	}
}

func (m *Method) DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error) {
	ssConn := &clientConn{
		Conn:        conn,
		method:      m,
		destination: destination,
	}
	return ssConn, ssConn.writeRequest(nil)
}

func (m *Method) DialEarlyConn(conn net.Conn, destination M.Socksaddr) net.Conn {
	return &clientConn{
		Conn:        conn,
		method:      m,
		destination: destination,
	}
}

func (m *Method) DialPacketConn(conn net.Conn) N.NetPacketConn {
	return &clientPacketConn{
		AbstractConn: conn,
		reader:       bufio.NewExtendedReader(conn),
		writer:       bufio.NewExtendedWriter(conn),
		method:       m,
	}
}

var _ N.ExtendedConn = (*clientConn)(nil)

type clientConn struct {
	net.Conn
	method          *Method
	destination     M.Socksaddr
	reader          *shadowio.Reader
	readWaitOptions N.ReadWaitOptions
	writer          *shadowio.Writer
	shadowio.WriterInterface
}

func (c *clientConn) writeRequest(payload []byte) error {
	requestBuffer := buf.New()
	requestBuffer.WriteRandom(c.method.keySaltLength)
	key := make([]byte, c.method.keySaltLength)
	legacykey.Kdf(c.method.key, requestBuffer.Bytes(), key)
	writeCipher, err := c.method.constructor(key)
	if err != nil {
		return err
	}
	bufferedRequestWriter := bufio.NewBufferedWriter(c.Conn, requestBuffer)
	requestContentWriter := shadowio.NewWriter(bufferedRequestWriter, writeCipher, nil, MaxPacketSize)
	bufferedRequestContentWriter := bufio.NewBufferedWriter(requestContentWriter, buf.New())
	err = M.SocksaddrSerializer.WriteAddrPort(bufferedRequestContentWriter, c.destination)
	if err != nil {
		return err
	}
	_, err = bufferedRequestContentWriter.Write(payload)
	if err != nil {
		return err
	}
	err = bufferedRequestContentWriter.Fallthrough()
	if err != nil {
		return err
	}
	err = bufferedRequestWriter.Fallthrough()
	if err != nil {
		return err
	}
	c.writer = shadowio.NewWriter(c.Conn, writeCipher, requestContentWriter.TakeNonce(), MaxPacketSize)
	return nil
}

func (c *clientConn) readResponse() error {
	buffer := buf.NewSize(c.method.keySaltLength)
	defer buffer.Release()
	_, err := buffer.ReadFullFrom(c.Conn, c.method.keySaltLength)
	if err != nil {
		return err
	}
	legacykey.Kdf(c.method.key, buffer.Bytes(), buffer.Bytes())
	readCipher, err := c.method.constructor(buffer.Bytes())
	if err != nil {
		return err
	}
	reader := shadowio.NewReader(c.Conn, readCipher)
	reader.InitializeReadWaiter(c.readWaitOptions)
	c.reader = reader
	return nil
}

func (c *clientConn) Read(p []byte) (n int, err error) {
	if c.reader == nil {
		err = c.readResponse()
		if err != nil {
			return
		}
	}
	return c.reader.Read(p)
}

func (c *clientConn) ReadBuffer(buffer *buf.Buffer) error {
	if c.reader == nil {
		err := c.readResponse()
		if err != nil {
			return err
		}
	}
	return c.reader.ReadBuffer(buffer)
}

func (c *clientConn) Write(p []byte) (n int, err error) {
	if c.writer == nil {
		err = c.writeRequest(p)
		if err == nil {
			n = len(p)
		}
		return
	}
	return c.writer.Write(p)
}

func (c *clientConn) WriteBuffer(buffer *buf.Buffer) error {
	if c.writer == nil {
		defer buffer.Release()
		return c.writeRequest(buffer.Bytes())
	}
	return c.writer.WriteBuffer(buffer)
}

func (c *clientConn) NeedHandshake() bool {
	return c.writer == nil
}

func (c *clientConn) Upstream() any {
	return c.Conn
}

func (c *clientConn) WriterMTU() int {
	return MaxPacketSize
}

type clientPacketConn struct {
	N.AbstractConn
	reader N.ExtendedReader
	writer N.ExtendedWriter
	method *Method
}

func (c *clientPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	err = c.reader.ReadBuffer(buffer)
	if err != nil {
		return
	}
	return c.readPacket(buffer)
}

func (c *clientPacketConn) readPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	if buffer.Len() < c.method.keySaltLength {
		return M.Socksaddr{}, C.ErrPacketTooShort
	}
	key := buf.NewSize(c.method.keySaltLength)
	legacykey.Kdf(c.method.key, buffer.To(c.method.keySaltLength), key.Extend(c.method.keySaltLength))
	readCipher, err := c.method.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return
	}
	packet, err := readCipher.Open(buffer.Index(c.method.keySaltLength), rw.ZeroBytes[:readCipher.NonceSize()], buffer.From(c.method.keySaltLength), nil)
	if err != nil {
		return
	}
	buffer.Advance(c.method.keySaltLength)
	buffer.Truncate(len(packet))
	if err != nil {
		return
	}
	destination, err = M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		return
	}
	return destination.Unwrap(), nil
}

func (c *clientPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	header := buf.With(buffer.ExtendHeader(c.method.keySaltLength + M.SocksaddrSerializer.AddrPortLen(destination)))
	header.WriteRandom(c.method.keySaltLength)
	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		return err
	}
	key := buf.NewSize(c.method.keySaltLength)
	legacykey.Kdf(c.method.key, header.To(c.method.keySaltLength), key.Extend(c.method.keySaltLength))
	writeCipher, err := c.method.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return err
	}
	writeCipher.Seal(buffer.Index(c.method.keySaltLength), rw.ZeroBytes[:writeCipher.NonceSize()], buffer.From(c.method.keySaltLength), nil)
	buffer.Extend(shadowio.Overhead)
	return c.writer.WriteBuffer(buffer)
}

func (c *clientPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	n, err = c.reader.Read(p)
	if err != nil {
		return
	}
	if n < c.method.keySaltLength {
		err = C.ErrPacketTooShort
		return
	}
	key := buf.NewSize(c.method.keySaltLength)
	legacykey.Kdf(c.method.key, p[:c.method.keySaltLength], key.Extend(c.method.keySaltLength))
	readCipher, err := c.method.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return
	}
	packet, err := readCipher.Open(p[c.method.keySaltLength:c.method.keySaltLength], rw.ZeroBytes[:readCipher.NonceSize()], p[c.method.keySaltLength:n], nil)
	if err != nil {
		return
	}
	packetContent := buf.As(packet)
	destination, err := M.SocksaddrSerializer.ReadAddrPort(packetContent)
	if err != nil {
		return
	}
	if !destination.IsFqdn() {
		addr = destination.UDPAddr()
	} else {
		addr = destination
	}
	n = copy(p, packetContent.Bytes())
	return
}

func (c *clientPacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	destination := M.SocksaddrFromNet(addr)
	buffer := buf.NewSize(c.method.keySaltLength + M.SocksaddrSerializer.AddrPortLen(destination) + len(p) + shadowio.Overhead)
	defer buffer.Release()
	buffer.WriteRandom(c.method.keySaltLength)
	err = M.SocksaddrSerializer.WriteAddrPort(buffer, destination)
	if err != nil {
		return
	}
	common.Must1(buffer.Write(p))
	key := buf.NewSize(c.method.keySaltLength)
	legacykey.Kdf(c.method.key, buffer.To(c.method.keySaltLength), key.Extend(c.method.keySaltLength))
	writeCipher, err := c.method.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return
	}
	writeCipher.Seal(buffer.Index(c.method.keySaltLength), rw.ZeroBytes[:writeCipher.NonceSize()], buffer.From(c.method.keySaltLength), nil)
	buffer.Extend(shadowio.Overhead)
	_, err = c.writer.Write(buffer.Bytes())
	if err != nil {
		return
	}
	return len(p), nil
}

func (c *clientPacketConn) FrontHeadroom() int {
	return c.method.keySaltLength + M.MaxSocksaddrLength
}

func (c *clientPacketConn) RearHeadroom() int {
	return shadowio.Overhead
}

func (c *clientPacketConn) ReaderMTU() int {
	return MaxPacketSize
}

func (c *clientPacketConn) WriterMTU() int {
	return MaxPacketSize
}

func (c *clientPacketConn) Upstream() any {
	return c.AbstractConn
}
