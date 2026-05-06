package shadowstream

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/md5"
	"crypto/rc4"
	"net"
	"os"

	C "github.com/sagernet/sing-shadowsocks2/cipher"
	"github.com/sagernet/sing-shadowsocks2/internal/legacykey"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"

	"golang.org/x/crypto/chacha20"
)

var MethodList = []string{
	"aes-128-ctr",
	"aes-192-ctr",
	"aes-256-ctr",
	"aes-128-cfb",
	"aes-192-cfb",
	"aes-256-cfb",
	"rc4-md5",
	"chacha20-ietf",
	"xchacha20",
}

func init() {
	C.RegisterMethod(MethodList, NewMethod)
}

type Method struct {
	keyLength          int
	saltLength         int
	encryptConstructor func(key []byte, salt []byte) (cipher.Stream, error)
	decryptConstructor func(key []byte, salt []byte) (cipher.Stream, error)
	key                []byte
}

func NewMethod(ctx context.Context, methodName string, options C.MethodOptions) (C.Method, error) {
	m := &Method{}
	switch methodName {
	case "aes-128-ctr":
		m.keyLength = 16
		m.saltLength = aes.BlockSize
		m.encryptConstructor = blockStream(aes.NewCipher, cipher.NewCTR)
		m.decryptConstructor = blockStream(aes.NewCipher, cipher.NewCTR)
	case "aes-192-ctr":
		m.keyLength = 24
		m.saltLength = aes.BlockSize
		m.encryptConstructor = blockStream(aes.NewCipher, cipher.NewCTR)
		m.decryptConstructor = blockStream(aes.NewCipher, cipher.NewCTR)
	case "aes-256-ctr":
		m.keyLength = 32
		m.saltLength = aes.BlockSize
		m.encryptConstructor = blockStream(aes.NewCipher, cipher.NewCTR)
		m.decryptConstructor = blockStream(aes.NewCipher, cipher.NewCTR)
	case "aes-128-cfb":
		m.keyLength = 16
		m.saltLength = aes.BlockSize
		m.encryptConstructor = blockStream(aes.NewCipher, cipher.NewCFBEncrypter)
		m.decryptConstructor = blockStream(aes.NewCipher, cipher.NewCFBDecrypter)
	case "aes-192-cfb":
		m.keyLength = 24
		m.saltLength = aes.BlockSize
		m.encryptConstructor = blockStream(aes.NewCipher, cipher.NewCFBEncrypter)
		m.decryptConstructor = blockStream(aes.NewCipher, cipher.NewCFBDecrypter)
	case "aes-256-cfb":
		m.keyLength = 32
		m.saltLength = aes.BlockSize
		m.encryptConstructor = blockStream(aes.NewCipher, cipher.NewCFBEncrypter)
		m.decryptConstructor = blockStream(aes.NewCipher, cipher.NewCFBDecrypter)
	case "rc4-md5":
		m.keyLength = 16
		m.saltLength = 16
		m.encryptConstructor = func(key []byte, salt []byte) (cipher.Stream, error) {
			h := md5.New()
			h.Write(key)
			h.Write(salt)
			return rc4.NewCipher(h.Sum(nil))
		}
		m.decryptConstructor = func(key []byte, salt []byte) (cipher.Stream, error) {
			h := md5.New()
			h.Write(key)
			h.Write(salt)
			return rc4.NewCipher(h.Sum(nil))
		}
	case "chacha20-ietf":
		m.keyLength = chacha20.KeySize
		m.saltLength = chacha20.NonceSize
		m.encryptConstructor = func(key []byte, salt []byte) (cipher.Stream, error) {
			return chacha20.NewUnauthenticatedCipher(key, salt)
		}
		m.decryptConstructor = func(key []byte, salt []byte) (cipher.Stream, error) {
			return chacha20.NewUnauthenticatedCipher(key, salt)
		}
	case "xchacha20":
		m.keyLength = chacha20.KeySize
		m.saltLength = chacha20.NonceSizeX
		m.encryptConstructor = func(key []byte, salt []byte) (cipher.Stream, error) {
			return chacha20.NewUnauthenticatedCipher(key, salt)
		}
		m.decryptConstructor = func(key []byte, salt []byte) (cipher.Stream, error) {
			return chacha20.NewUnauthenticatedCipher(key, salt)
		}
	default:
		return nil, os.ErrInvalid
	}
	if len(options.Key) == m.keyLength {
		m.key = options.Key
	} else if len(options.Key) > 0 {
		return nil, E.New("bad key length, required ", m.keyLength, ", got ", len(options.Key))
	} else if options.Password != "" {
		m.key = legacykey.Key([]byte(options.Password), m.keyLength)
	} else {
		return nil, C.ErrMissingPassword
	}
	return m, nil
}

func blockStream(blockCreator func(key []byte) (cipher.Block, error), streamCreator func(block cipher.Block, iv []byte) cipher.Stream) func([]byte, []byte) (cipher.Stream, error) {
	return func(key []byte, iv []byte) (cipher.Stream, error) {
		block, err := blockCreator(key)
		if err != nil {
			return nil, err
		}
		return streamCreator(block, iv), err
	}
}

func (m *Method) DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error) {
	ssConn := &clientConn{
		ExtendedConn: bufio.NewExtendedConn(conn),
		method:       m,
		destination:  destination,
	}
	return ssConn, common.Error(ssConn.Write(nil))
}

func (m *Method) DialEarlyConn(conn net.Conn, destination M.Socksaddr) net.Conn {
	return &clientConn{
		ExtendedConn: bufio.NewExtendedConn(conn),
		method:       m,
		destination:  destination,
	}
}

func (m *Method) DialPacketConn(conn net.Conn) N.NetPacketConn {
	return &clientPacketConn{
		ExtendedConn: bufio.NewExtendedConn(conn),
		method:       m,
	}
}

type clientConn struct {
	N.ExtendedConn
	method      *Method
	destination M.Socksaddr
	readStream  cipher.Stream
	writeStream cipher.Stream
}

func (c *clientConn) readResponse() error {
	saltBuffer := buf.NewSize(c.method.saltLength)
	defer saltBuffer.Release()
	_, err := saltBuffer.ReadFullFrom(c.ExtendedConn, c.method.saltLength)
	if err != nil {
		return err
	}
	c.readStream, err = c.method.decryptConstructor(c.method.key, saltBuffer.Bytes())
	return err
}

func (c *clientConn) Read(p []byte) (n int, err error) {
	if c.readStream == nil {
		err = c.readResponse()
		if err != nil {
			return
		}
	}
	n, err = c.ExtendedConn.Read(p)
	if err != nil {
		return
	}
	c.readStream.XORKeyStream(p[:n], p[:n])
	return
}

func (c *clientConn) Write(p []byte) (n int, err error) {
	if c.writeStream == nil {
		buffer := buf.NewSize(c.method.saltLength + M.SocksaddrSerializer.AddrPortLen(c.destination) + len(p))
		defer buffer.Release()
		buffer.WriteRandom(c.method.saltLength)
		err = M.SocksaddrSerializer.WriteAddrPort(buffer, c.destination)
		if err != nil {
			return
		}
		common.Must1(buffer.Write(p))
		c.writeStream, err = c.method.encryptConstructor(c.method.key, buffer.To(c.method.saltLength))
		if err != nil {
			return
		}
		c.writeStream.XORKeyStream(buffer.From(c.method.saltLength), buffer.From(c.method.saltLength))
		_, err = c.ExtendedConn.Write(buffer.Bytes())
		if err == nil {
			n = len(p)
		}
		return
	}
	c.writeStream.XORKeyStream(p, p)
	return c.ExtendedConn.Write(p)
}

func (c *clientConn) ReadBuffer(buffer *buf.Buffer) error {
	if c.readStream == nil {
		err := c.readResponse()
		if err != nil {
			return err
		}
	}
	err := c.ExtendedConn.ReadBuffer(buffer)
	if err != nil {
		return err
	}
	c.readStream.XORKeyStream(buffer.Bytes(), buffer.Bytes())
	return nil
}

func (c *clientConn) WriteBuffer(buffer *buf.Buffer) error {
	if c.writeStream == nil {
		header := buf.With(buffer.ExtendHeader(c.method.saltLength + M.SocksaddrSerializer.AddrPortLen(c.destination)))
		header.WriteRandom(c.method.saltLength)
		err := M.SocksaddrSerializer.WriteAddrPort(header, c.destination)
		if err != nil {
			return err
		}
		c.writeStream, err = c.method.encryptConstructor(c.method.key, header.To(c.method.saltLength))
		if err != nil {
			return err
		}
		c.writeStream.XORKeyStream(buffer.From(c.method.saltLength), buffer.From(c.method.saltLength))
	} else {
		c.writeStream.XORKeyStream(buffer.Bytes(), buffer.Bytes())
	}
	return c.ExtendedConn.WriteBuffer(buffer)
}

func (c *clientConn) FrontHeadroom() int {
	if c.writeStream == nil {
		return c.method.saltLength + M.SocksaddrSerializer.AddrPortLen(c.destination)
	}
	return 0
}

func (c *clientConn) NeedHandshake() bool {
	return c.writeStream == nil
}

func (c *clientConn) Upstream() any {
	return c.ExtendedConn
}

type clientPacketConn struct {
	N.ExtendedConn
	method *Method
}

func (c *clientPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	err = c.ReadBuffer(buffer)
	if err != nil {
		return
	}
	stream, err := c.method.decryptConstructor(c.method.key, buffer.To(c.method.saltLength))
	if err != nil {
		return
	}
	stream.XORKeyStream(buffer.From(c.method.saltLength), buffer.From(c.method.saltLength))
	buffer.Advance(c.method.saltLength)
	destination, err = M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		return
	}
	return destination.Unwrap(), nil
}

func (c *clientPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	header := buf.With(buffer.ExtendHeader(c.method.saltLength + M.SocksaddrSerializer.AddrPortLen(destination)))
	header.WriteRandom(c.method.saltLength)
	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		return err
	}
	stream, err := c.method.encryptConstructor(c.method.key, buffer.To(c.method.saltLength))
	if err != nil {
		return err
	}
	stream.XORKeyStream(buffer.From(c.method.saltLength), buffer.From(c.method.saltLength))
	return c.ExtendedConn.WriteBuffer(buffer)
}

func (c *clientPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	n, err = c.ExtendedConn.Read(p)
	if err != nil {
		return
	}
	stream, err := c.method.decryptConstructor(c.method.key, p[:c.method.saltLength])
	if err != nil {
		return
	}
	buffer := buf.As(p[c.method.saltLength:n])
	stream.XORKeyStream(buffer.Bytes(), buffer.Bytes())
	destination, err := M.SocksaddrSerializer.ReadAddrPort(buffer)
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
	buffer := buf.NewSize(c.method.saltLength + M.SocksaddrSerializer.AddrPortLen(destination) + len(p))
	defer buffer.Release()
	buffer.WriteRandom(c.method.saltLength)
	err = M.SocksaddrSerializer.WriteAddrPort(buffer, destination)
	if err != nil {
		return
	}
	stream, err := c.method.encryptConstructor(c.method.key, buffer.To(c.method.saltLength))
	if err != nil {
		return
	}
	stream.XORKeyStream(buffer.From(c.method.saltLength), buffer.From(c.method.saltLength))
	stream.XORKeyStream(buffer.Extend(len(p)), p)
	_, err = c.ExtendedConn.Write(buffer.Bytes())
	if err == nil {
		n = len(p)
	}
	return
}

func (c *clientPacketConn) FrontHeadroom() int {
	return c.method.saltLength + M.MaxSocksaddrLength
}

func (c *clientPacketConn) Upstream() any {
	return c.ExtendedConn
}
