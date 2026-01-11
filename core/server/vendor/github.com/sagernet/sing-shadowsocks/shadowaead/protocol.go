package shadowaead

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/sha1"
	"io"
	"net"

	"github.com/sagernet/sing-shadowsocks"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/rw"

	"golang.org/x/crypto/chacha20poly1305"
	"golang.org/x/crypto/hkdf"
)

var List = []string{
	"aes-128-gcm",
	"aes-192-gcm",
	"aes-256-gcm",
	"chacha20-ietf-poly1305",
	"xchacha20-ietf-poly1305",
}

var _ shadowsocks.Method = (*Method)(nil)

func New(method string, key []byte, password string) (*Method, error) {
	m := &Method{
		name: method,
	}
	switch method {
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
	if len(key) == m.keySaltLength {
		m.key = key
	} else if len(key) > 0 {
		return nil, shadowsocks.ErrBadKey
	} else if password == "" {
		return nil, shadowsocks.ErrMissingPassword
	} else {
		m.key = shadowsocks.Key([]byte(password), m.keySaltLength)
	}
	return m, nil
}

func Kdf(key, iv []byte, buffer *buf.Buffer) {
	kdf := hkdf.New(sha1.New, key, iv, []byte("ss-subkey"))
	common.Must1(buffer.ReadFullFrom(kdf, buffer.FreeLen()))
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

type Method struct {
	name          string
	keySaltLength int
	constructor   func(key []byte) (cipher.AEAD, error)
	key           []byte
}

func (m *Method) Name() string {
	return m.name
}

func (m *Method) DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error) {
	shadowsocksConn := &clientConn{
		Conn:        conn,
		Method:      m,
		destination: destination,
	}
	return shadowsocksConn, shadowsocksConn.writeRequest(nil)
}

func (m *Method) DialEarlyConn(conn net.Conn, destination M.Socksaddr) net.Conn {
	return &clientConn{
		Conn:        conn,
		Method:      m,
		destination: destination,
	}
}

func (m *Method) DialPacketConn(conn net.Conn) N.NetPacketConn {
	return &clientPacketConn{m, conn}
}

type clientConn struct {
	net.Conn
	*Method
	destination M.Socksaddr
	reader      *Reader
	writer      *Writer
}

func (c *clientConn) writeRequest(payload []byte) error {
	salt := buf.NewSize(c.keySaltLength)
	defer salt.Release()
	salt.WriteRandom(c.keySaltLength)

	key := buf.NewSize(c.keySaltLength)

	Kdf(c.key, salt.Bytes(), key)
	writeCipher, err := c.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return err
	}
	writer := NewWriter(c.Conn, writeCipher, MaxPacketSize)
	header := writer.Buffer()
	common.Must1(header.Write(salt.Bytes()))
	bufferedWriter := writer.BufferedWriter(header.Len())

	if len(payload) > 0 {
		err = M.SocksaddrSerializer.WriteAddrPort(bufferedWriter, c.destination)
		if err != nil {
			return err
		}

		_, err = bufferedWriter.Write(payload)
		if err != nil {
			return err
		}
	} else {
		err = M.SocksaddrSerializer.WriteAddrPort(bufferedWriter, c.destination)
		if err != nil {
			return err
		}
	}

	err = bufferedWriter.Flush()
	if err != nil {
		return err
	}

	c.writer = writer
	return nil
}

func (c *clientConn) readResponse() error {
	salt := buf.NewSize(c.keySaltLength)
	defer salt.Release()
	_, err := salt.ReadFullFrom(c.Conn, c.keySaltLength)
	if err != nil {
		return err
	}
	key := buf.NewSize(c.keySaltLength)
	defer key.Release()
	Kdf(c.key, salt.Bytes(), key)
	readCipher, err := c.constructor(key.Bytes())
	if err != nil {
		return err
	}
	c.reader = NewReader(
		c.Conn,
		readCipher,
		MaxPacketSize,
	)
	return nil
}

func (c *clientConn) Read(p []byte) (n int, err error) {
	if c.reader == nil {
		if err = c.readResponse(); err != nil {
			return
		}
	}
	return c.reader.Read(p)
}

func (c *clientConn) WriteTo(w io.Writer) (n int64, err error) {
	if c.reader == nil {
		if err = c.readResponse(); err != nil {
			return
		}
	}
	return c.reader.WriteTo(w)
}

func (c *clientConn) Write(p []byte) (n int, err error) {
	if c.writer == nil {
		err = c.writeRequest(p)
		if err != nil {
			return
		}
		return len(p), nil
	}
	return c.writer.Write(p)
}

func (c *clientConn) NeedHandshake() bool {
	return c.writer == nil
}

func (c *clientConn) NeedAdditionalReadDeadline() bool {
	return true
}

func (c *clientConn) Upstream() any {
	return c.Conn
}

type clientPacketConn struct {
	*Method
	net.Conn
}

func (c *clientPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	defer buffer.Release()
	header := buf.With(buffer.ExtendHeader(c.keySaltLength + M.SocksaddrSerializer.AddrPortLen(destination)))
	header.WriteRandom(c.keySaltLength)
	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		return err
	}
	key := buf.NewSize(c.keySaltLength)
	Kdf(c.key, buffer.To(c.keySaltLength), key)
	writeCipher, err := c.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return err
	}
	writeCipher.Seal(buffer.Index(c.keySaltLength), rw.ZeroBytes[:writeCipher.NonceSize()], buffer.From(c.keySaltLength), nil)
	buffer.Extend(Overhead)
	return common.Error(c.Write(buffer.Bytes()))
}

func (c *clientPacketConn) ReadPacket(buffer *buf.Buffer) (M.Socksaddr, error) {
	n, err := c.Read(buffer.FreeBytes())
	if err != nil {
		return M.Socksaddr{}, err
	}
	buffer.Truncate(n)
	if buffer.Len() < c.keySaltLength {
		return M.Socksaddr{}, io.ErrShortBuffer
	}
	key := buf.NewSize(c.keySaltLength)
	Kdf(c.key, buffer.To(c.keySaltLength), key)
	readCipher, err := c.constructor(key.Bytes())
	key.Release()
	if err != nil {
		return M.Socksaddr{}, err
	}
	packet, err := readCipher.Open(buffer.Index(c.keySaltLength), rw.ZeroBytes[:readCipher.NonceSize()], buffer.From(c.keySaltLength), nil)
	if err != nil {
		return M.Socksaddr{}, err
	}
	buffer.Advance(c.keySaltLength)
	buffer.Truncate(len(packet))
	if err != nil {
		return M.Socksaddr{}, err
	}
	return M.SocksaddrSerializer.ReadAddrPort(buffer)
}

func (c *clientPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	buffer := buf.With(p)
	destination, err := c.ReadPacket(buffer)
	if err != nil {
		return
	}
	if destination.IsFqdn() {
		addr = destination
	} else {
		addr = destination.UDPAddr()
	}
	n = copy(p, buffer.Bytes())
	return
}

func (c *clientPacketConn) WriteTo(p []byte, addr net.Addr) (n int, err error) {
	destination := M.SocksaddrFromNet(addr)
	buffer := buf.NewSize(c.keySaltLength + M.SocksaddrSerializer.AddrPortLen(destination) + len(p) + Overhead)
	defer buffer.Release()
	buffer.Resize(c.keySaltLength+M.SocksaddrSerializer.AddrPortLen(destination), 0)
	common.Must1(buffer.Write(p))
	err = c.WritePacket(buffer, destination)
	if err != nil {
		return
	}
	return len(p), nil
}

func (c *clientPacketConn) FrontHeadroom() int {
	return c.keySaltLength + M.MaxSocksaddrLength
}

func (c *clientPacketConn) RearHeadroom() int {
	return Overhead
}

func (c *clientPacketConn) ReaderMTU() int {
	return MaxPacketSize
}

func (c *clientPacketConn) WriterMTU() int {
	return MaxPacketSize
}

func (c *clientPacketConn) Upstream() any {
	return c.Conn
}
