package shadowaead_2022

import (
	"bytes"
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"encoding/binary"
	"math"
	mRand "math/rand"
	"net"
	"os"
	"strings"
	"time"

	C "github.com/sagernet/sing-shadowsocks2/cipher"
	"github.com/sagernet/sing-shadowsocks2/internal/shadowio"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/ntp"

	"golang.org/x/crypto/chacha20poly1305"
	"lukechampine.com/blake3"
)

var MethodList = []string{
	"2022-blake3-aes-128-gcm",
	"2022-blake3-aes-256-gcm",
	"2022-blake3-chacha20-poly1305",
}

func init() {
	C.RegisterMethod(MethodList, NewMethod)
}

type Method struct {
	keySaltLength         int
	timeFunc              func() time.Time
	constructor           func(key []byte) (cipher.AEAD, error)
	blockConstructor      func(key []byte) (cipher.Block, error)
	udpCipher             cipher.AEAD
	udpBlockEncryptCipher cipher.Block
	udpBlockDecryptCipher cipher.Block
	pskList               [][]byte
	pskHash               []byte
}

func NewMethod(ctx context.Context, methodName string, options C.MethodOptions) (C.Method, error) {
	m := &Method{
		timeFunc: ntp.TimeFuncFromContext(ctx),
		pskList:  options.KeyList,
	}
	if options.Password != "" {
		var pskList [][]byte
		keyStrList := strings.Split(options.Password, ":")
		pskList = make([][]byte, len(keyStrList))
		for i, keyStr := range keyStrList {
			kb, err := base64.StdEncoding.DecodeString(keyStr)
			if err != nil {
				return nil, E.Cause(err, "decode key")
			}
			pskList[i] = kb
		}
		m.pskList = pskList
	}
	switch methodName {
	case "2022-blake3-aes-128-gcm":
		m.keySaltLength = 16
		m.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
		m.blockConstructor = aes.NewCipher
	case "2022-blake3-aes-256-gcm":
		m.keySaltLength = 32
		m.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
		m.blockConstructor = aes.NewCipher
	case "2022-blake3-chacha20-poly1305":
		if len(m.pskList) > 1 {
			return nil, ErrNoEIH
		}
		m.keySaltLength = 32
		m.constructor = chacha20poly1305.New
	default:
		return nil, os.ErrInvalid
	}
	if len(m.pskList) == 0 {
		return nil, C.ErrMissingPassword
	}
	for _, key := range m.pskList {
		if len(key) != m.keySaltLength {
			return nil, E.New("bad key length, required ", m.keySaltLength, ", got ", len(key))
		}
	}
	if len(m.pskList) > 1 {
		pskHash := make([]byte, (len(m.pskList)-1)*aes.BlockSize)
		for i, key := range m.pskList {
			if i == 0 {
				continue
			}
			keyHash := blake3.Sum512(key)
			copy(pskHash[aes.BlockSize*(i-1):aes.BlockSize*i], keyHash[:aes.BlockSize])
		}
		m.pskHash = pskHash
	}
	var err error
	switch methodName {
	case "2022-blake3-aes-128-gcm", "2022-blake3-aes-256-gcm":
		m.udpBlockEncryptCipher, err = aes.NewCipher(m.pskList[0])
		if err != nil {
			return nil, err
		}
		m.udpBlockDecryptCipher, err = aes.NewCipher(m.pskList[len(m.pskList)-1])
		if err != nil {
			return nil, err
		}
	case "2022-blake3-chacha20-poly1305":
		m.udpCipher, err = chacha20poly1305.NewX(m.pskList[0])
		if err != nil {
			return nil, err
		}
	}
	return m, nil
}

func (m *Method) DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error) {
	shadowsocksConn := &clientConn{
		Conn:        conn,
		method:      m,
		destination: destination,
	}
	return shadowsocksConn, shadowsocksConn.writeRequest(nil)
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
		session:      m.newUDPSession(),
	}
}

func (m *Method) time() time.Time {
	if m.timeFunc != nil {
		return m.timeFunc()
	} else {
		return time.Now()
	}
}

type clientConn struct {
	net.Conn
	method          *Method
	destination     M.Socksaddr
	requestSalt     []byte
	reader          *shadowio.Reader
	readWaitOptions N.ReadWaitOptions
	writer          *shadowio.Writer
	shadowio.WriterInterface
}

func (c *clientConn) writeRequest(payload []byte) error {
	requestSalt := make([]byte, c.method.keySaltLength)
	requestBuffer := buf.New()
	defer requestBuffer.Release()
	requestBuffer.WriteRandom(c.method.keySaltLength)
	copy(requestSalt, requestBuffer.Bytes())
	key := SessionKey(c.method.pskList[len(c.method.pskList)-1], requestSalt, c.method.keySaltLength)
	writeCipher, err := c.method.constructor(key)
	if err != nil {
		return err
	}
	writer := shadowio.NewWriter(
		c.Conn,
		writeCipher,
		nil,
		buf.BufferSize-shadowio.PacketLengthBufferSize-shadowio.Overhead*2,
	)
	err = c.method.writeExtendedIdentityHeaders(requestBuffer, requestBuffer.To(c.method.keySaltLength))
	if err != nil {
		return err
	}
	fixedLengthBuffer := buf.With(requestBuffer.Extend(RequestHeaderFixedChunkLength + shadowio.Overhead))
	common.Must(fixedLengthBuffer.WriteByte(HeaderTypeClient))
	common.Must(binary.Write(fixedLengthBuffer, binary.BigEndian, uint64(c.method.time().Unix())))
	variableLengthHeaderLen := M.SocksaddrSerializer.AddrPortLen(c.destination) + 2
	var paddingLen int
	if len(payload) < MaxPaddingLength {
		paddingLen = mRand.Intn(MaxPaddingLength) + 1
	}
	variableLengthHeaderLen += paddingLen
	maxPayloadLen := requestBuffer.FreeLen() - (variableLengthHeaderLen + shadowio.Overhead)
	payloadLen := len(payload)
	if payloadLen > maxPayloadLen {
		payloadLen = maxPayloadLen
	}
	variableLengthHeaderLen += payloadLen
	common.Must(binary.Write(fixedLengthBuffer, binary.BigEndian, uint16(variableLengthHeaderLen)))
	writer.Encrypt(fixedLengthBuffer.Index(0), fixedLengthBuffer.Bytes())
	fixedLengthBuffer.Extend(shadowio.Overhead)

	variableLengthBuffer := buf.With(requestBuffer.Extend(variableLengthHeaderLen + shadowio.Overhead))
	err = M.SocksaddrSerializer.WriteAddrPort(variableLengthBuffer, c.destination)
	if err != nil {
		return err
	}
	common.Must(binary.Write(variableLengthBuffer, binary.BigEndian, uint16(paddingLen)))
	if paddingLen > 0 {
		variableLengthBuffer.Extend(paddingLen)
	}
	if payloadLen > 0 {
		common.Must1(variableLengthBuffer.Write(payload[:payloadLen]))
	}
	writer.Encrypt(variableLengthBuffer.Index(0), variableLengthBuffer.Bytes())
	variableLengthBuffer.Extend(shadowio.Overhead)
	_, err = c.Conn.Write(requestBuffer.Bytes())
	if err != nil {
		return err
	}
	if len(payload) > payloadLen {
		_, err = writer.Write(payload[payloadLen:])
		if err != nil {
			return err
		}
	}
	c.requestSalt = requestSalt
	c.writer = writer
	return nil
}

func (m *Method) writeExtendedIdentityHeaders(request *buf.Buffer, salt []byte) error {
	pskLen := len(m.pskList)
	if pskLen < 2 {
		return nil
	}
	for i, psk := range m.pskList {
		keyMaterial := make([]byte, m.keySaltLength*2)
		copy(keyMaterial, psk)
		copy(keyMaterial[m.keySaltLength:], salt)
		identitySubkey := make([]byte, m.keySaltLength)
		blake3.DeriveKey(identitySubkey, "shadowsocks 2022 identity subkey", keyMaterial)
		pskHash := m.pskHash[aes.BlockSize*i : aes.BlockSize*(i+1)]
		header := request.Extend(16)
		b, err := m.blockConstructor(identitySubkey)
		if err != nil {
			return err
		}
		b.Encrypt(header, pskHash)
		if i == pskLen-2 {
			break
		}
	}
	return nil
}

func (c *clientConn) readResponse() error {
	salt := buf.NewSize(c.method.keySaltLength)
	defer salt.Release()
	_, err := salt.ReadFullFrom(c.Conn, c.method.keySaltLength)
	if err != nil {
		return err
	}
	key := SessionKey(c.method.pskList[len(c.method.pskList)-1], salt.Bytes(), c.method.keySaltLength)
	readCipher, err := c.method.constructor(key)
	if err != nil {
		return err
	}
	reader := shadowio.NewReader(c.Conn, readCipher)
	fixedResponseBuffer, err := reader.ReadFixedBuffer(1 + 8 + c.method.keySaltLength + 2)
	if err != nil {
		return err
	}
	headerType := common.Must1(fixedResponseBuffer.ReadByte())
	if headerType != HeaderTypeServer {
		return E.Extend(ErrBadHeaderType, "expected ", HeaderTypeServer, ", got ", headerType)
	}
	var epoch uint64
	common.Must(binary.Read(fixedResponseBuffer, binary.BigEndian, &epoch))
	diff := int(math.Abs(float64(c.method.time().Unix() - int64(epoch))))
	if diff > 30 {
		return E.Extend(ErrBadTimestamp, "received ", epoch, ", diff ", diff, "s")
	}
	responseSalt := common.Must1(fixedResponseBuffer.ReadBytes(c.method.keySaltLength))
	if !bytes.Equal(responseSalt, c.requestSalt) {
		return ErrBadRequestSalt
	}
	var length uint16
	common.Must(binary.Read(reader, binary.BigEndian, &length))
	_, err = reader.ReadFixedBuffer(int(length))
	if err != nil {
		return err
	}
	reader.InitializeReadWaiter(c.readWaitOptions)
	c.reader = reader
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

func (c *clientConn) Close() error {
	return common.Close(
		c.Conn,
		common.PtrOrNil(c.reader),
		common.PtrOrNil(c.writer),
	)
}

type clientPacketConn struct {
	N.AbstractConn
	reader  N.ExtendedReader
	writer  N.ExtendedWriter
	method  *Method
	session *udpSession
}

func (m *Method) newUDPSession() *udpSession {
	session := &udpSession{}
	if m.udpCipher != nil {
		session.rng = Blake3KeyedHash(rand.Reader)
		common.Must(binary.Read(session.rng, binary.BigEndian, &session.sessionId))
	} else {
		common.Must(binary.Read(rand.Reader, binary.BigEndian, &session.sessionId))
	}
	session.packetId--
	if m.udpCipher == nil {
		sessionId := make([]byte, 8)
		binary.BigEndian.PutUint64(sessionId, session.sessionId)
		key := SessionKey(m.pskList[len(m.pskList)-1], sessionId, m.keySaltLength)
		var err error
		session.cipher, err = m.constructor(key)
		if err != nil {
			return nil
		}
	}
	return session
}

func (c *clientPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	var hdrLen int
	if c.method.udpCipher != nil {
		hdrLen = PacketNonceSize
	}

	var paddingLen int
	if destination.Port == 53 && buffer.Len() < MaxPaddingLength {
		paddingLen = mRand.Intn(MaxPaddingLength-buffer.Len()) + 1
	}

	hdrLen += 16 // packet header
	pskLen := len(c.method.pskList)
	if c.method.udpCipher == nil && pskLen > 1 {
		hdrLen += (pskLen - 1) * aes.BlockSize
	}
	hdrLen += 1 // header type
	hdrLen += 8 // timestamp
	hdrLen += 2 // padding length
	hdrLen += paddingLen
	hdrLen += M.SocksaddrSerializer.AddrPortLen(destination)
	header := buf.With(buffer.ExtendHeader(hdrLen))

	var dataIndex int
	if c.method.udpCipher != nil {
		common.Must1(header.ReadFullFrom(c.session.rng, PacketNonceSize))
		if pskLen > 1 {
			panic("unsupported chacha extended header")
		}
		dataIndex = PacketNonceSize
	} else {
		dataIndex = aes.BlockSize
	}

	common.Must(
		binary.Write(header, binary.BigEndian, c.session.sessionId),
		binary.Write(header, binary.BigEndian, c.session.nextPacketId()),
	)

	if c.method.udpCipher == nil && pskLen > 1 {
		for i, psk := range c.method.pskList {
			dataIndex += aes.BlockSize
			pskHash := c.method.pskHash[aes.BlockSize*i : aes.BlockSize*(i+1)]

			identityHeader := header.Extend(aes.BlockSize)
			xorWords(identityHeader, pskHash, header.To(aes.BlockSize))
			b, err := c.method.blockConstructor(psk)
			if err != nil {
				return err
			}
			b.Encrypt(identityHeader, identityHeader)

			if i == pskLen-2 {
				break
			}
		}
	}
	common.Must(
		header.WriteByte(HeaderTypeClient),
		binary.Write(header, binary.BigEndian, uint64(c.method.time().Unix())),
		binary.Write(header, binary.BigEndian, uint16(paddingLen)), // padding length
	)

	if paddingLen > 0 {
		header.Extend(paddingLen)
	}

	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		return err
	}
	if c.method.udpCipher != nil {
		c.method.udpCipher.Seal(buffer.Index(dataIndex), buffer.To(dataIndex), buffer.From(dataIndex), nil)
		buffer.Extend(shadowio.Overhead)
	} else {
		packetHeader := buffer.To(aes.BlockSize)
		c.session.cipher.Seal(buffer.Index(dataIndex), packetHeader[4:16], buffer.From(dataIndex), nil)
		buffer.Extend(shadowio.Overhead)
		c.method.udpBlockEncryptCipher.Encrypt(packetHeader, packetHeader)
	}
	return c.writer.WriteBuffer(buffer)
}

func (c *clientPacketConn) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	err = c.reader.ReadBuffer(buffer)
	if err != nil {
		return
	}
	return c.readPacket(buffer)
}

func (c *clientPacketConn) readPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	var packetHeader []byte
	if c.method.udpCipher != nil {
		if buffer.Len() < PacketNonceSize+PacketMinimalHeaderSize {
			return M.Socksaddr{}, C.ErrPacketTooShort
		}
		_, err = c.method.udpCipher.Open(buffer.Index(PacketNonceSize), buffer.To(PacketNonceSize), buffer.From(PacketNonceSize), nil)
		if err != nil {
			return M.Socksaddr{}, E.Cause(err, "decrypt packet")
		}
		buffer.Advance(PacketNonceSize)
		buffer.Truncate(buffer.Len() - shadowio.Overhead)
	} else {
		if buffer.Len() < PacketMinimalHeaderSize {
			return M.Socksaddr{}, C.ErrPacketTooShort
		}
		packetHeader = buffer.To(aes.BlockSize)
		c.method.udpBlockDecryptCipher.Decrypt(packetHeader, packetHeader)
	}

	var sessionId, packetId uint64
	err = binary.Read(buffer, binary.BigEndian, &sessionId)
	if err != nil {
		return M.Socksaddr{}, err
	}
	err = binary.Read(buffer, binary.BigEndian, &packetId)
	if err != nil {
		return M.Socksaddr{}, err
	}

	if sessionId == c.session.remoteSessionId {
		if !c.session.window.Check(packetId) {
			return M.Socksaddr{}, ErrPacketIdNotUnique
		}
	} else if sessionId == c.session.lastRemoteSessionId {
		if !c.session.lastWindow.Check(packetId) {
			return M.Socksaddr{}, ErrPacketIdNotUnique
		}
	}

	var remoteCipher cipher.AEAD
	if packetHeader != nil {
		if sessionId == c.session.remoteSessionId {
			remoteCipher = c.session.remoteCipher
		} else if sessionId == c.session.lastRemoteSessionId {
			remoteCipher = c.session.lastRemoteCipher
		} else {
			key := SessionKey(c.method.pskList[len(c.method.pskList)-1], packetHeader[:8], c.method.keySaltLength)
			remoteCipher, err = c.method.constructor(key)
			if err != nil {
				return M.Socksaddr{}, err
			}
		}
		_, err = remoteCipher.Open(buffer.Index(0), packetHeader[4:16], buffer.Bytes(), nil)
		if err != nil {
			return M.Socksaddr{}, E.Cause(err, "decrypt packet")
		}
		buffer.Truncate(buffer.Len() - shadowio.Overhead)
	}

	var headerType byte
	headerType, err = buffer.ReadByte()
	if err != nil {
		return M.Socksaddr{}, err
	}
	if headerType != HeaderTypeServer {
		return M.Socksaddr{}, E.Extend(ErrBadHeaderType, "expected ", HeaderTypeServer, ", got ", headerType)
	}

	var epoch uint64
	err = binary.Read(buffer, binary.BigEndian, &epoch)
	if err != nil {
		return M.Socksaddr{}, err
	}

	diff := int(math.Abs(float64(c.method.time().Unix() - int64(epoch))))
	if diff > 30 {
		return M.Socksaddr{}, E.Extend(ErrBadTimestamp, "received ", epoch, ", diff ", diff, "s")
	}

	if sessionId == c.session.remoteSessionId {
		c.session.window.Add(packetId)
	} else if sessionId == c.session.lastRemoteSessionId {
		c.session.lastWindow.Add(packetId)
		c.session.lastRemoteSeen = c.method.time().Unix()
	} else {
		if c.session.remoteSessionId != 0 {
			if c.method.time().Unix()-c.session.lastRemoteSeen < 60 {
				return M.Socksaddr{}, ErrTooManyServerSessions
			} else {
				c.session.lastRemoteSessionId = c.session.remoteSessionId
				c.session.lastWindow = c.session.window
				c.session.lastRemoteSeen = c.method.time().Unix()
				c.session.lastRemoteCipher = c.session.remoteCipher
				c.session.window = SlidingWindow{}
			}
		}
		c.session.remoteSessionId = sessionId
		c.session.remoteCipher = remoteCipher
		c.session.window.Add(packetId)
	}

	var clientSessionId uint64
	err = binary.Read(buffer, binary.BigEndian, &clientSessionId)
	if err != nil {
		return M.Socksaddr{}, err
	}

	if clientSessionId != c.session.sessionId {
		return M.Socksaddr{}, ErrBadClientSessionId
	}

	var paddingLen uint16
	err = binary.Read(buffer, binary.BigEndian, &paddingLen)
	if err != nil {
		return M.Socksaddr{}, E.Cause(err, "read padding length")
	}
	buffer.Advance(int(paddingLen))

	destination, err = M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		return
	}
	return destination.Unwrap(), nil
}

func (c *clientPacketConn) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	n, err = c.reader.Read(p)
	if err != nil {
		return
	}
	buffer := buf.As(p[:n])
	destination, err := c.readPacket(buffer)
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
	var overHead int
	if c.method.udpCipher != nil {
		overHead = PacketNonceSize + shadowio.Overhead
	} else {
		overHead = shadowio.Overhead
	}
	overHead += 16 // packet header
	pskLen := len(c.method.pskList)
	if c.method.udpCipher == nil && pskLen > 1 {
		overHead += (pskLen - 1) * aes.BlockSize
	}
	var paddingLen int
	if destination.Port == 53 && len(p) < MaxPaddingLength {
		paddingLen = mRand.Intn(MaxPaddingLength-len(p)) + 1
	}
	overHead += 1 // header type
	overHead += 8 // timestamp
	overHead += 2 // padding length
	overHead += paddingLen
	overHead += M.SocksaddrSerializer.AddrPortLen(destination)

	buffer := buf.NewSize(overHead + len(p))
	defer buffer.Release()

	var dataIndex int
	if c.method.udpCipher != nil {
		common.Must1(buffer.ReadFullFrom(c.session.rng, PacketNonceSize))
		if pskLen > 1 {
			panic("unsupported chacha extended header")
		}
		dataIndex = PacketNonceSize
	} else {
		dataIndex = aes.BlockSize
	}

	common.Must(
		binary.Write(buffer, binary.BigEndian, c.session.sessionId),
		binary.Write(buffer, binary.BigEndian, c.session.nextPacketId()),
	)

	if c.method.udpCipher == nil && pskLen > 1 {
		for i, psk := range c.method.pskList {
			dataIndex += aes.BlockSize
			pskHash := c.method.pskHash[aes.BlockSize*i : aes.BlockSize*(i+1)]

			identityHeader := buffer.Extend(aes.BlockSize)
			xorWords(identityHeader, pskHash, buffer.To(aes.BlockSize))
			b, err := c.method.blockConstructor(psk)
			if err != nil {
				return 0, err
			}
			b.Encrypt(identityHeader, identityHeader)

			if i == pskLen-2 {
				break
			}
		}
	}
	common.Must(
		buffer.WriteByte(HeaderTypeClient),
		binary.Write(buffer, binary.BigEndian, uint64(c.method.time().Unix())),
		binary.Write(buffer, binary.BigEndian, uint16(paddingLen)), // padding length
	)

	if paddingLen > 0 {
		buffer.Extend(paddingLen)
	}

	err = M.SocksaddrSerializer.WriteAddrPort(buffer, destination)
	if err != nil {
		return
	}
	common.Must1(buffer.Write(p))
	if c.method.udpCipher != nil {
		c.method.udpCipher.Seal(buffer.Index(dataIndex), buffer.To(dataIndex), buffer.From(dataIndex), nil)
		buffer.Extend(shadowio.Overhead)
	} else {
		packetHeader := buffer.To(aes.BlockSize)
		c.session.cipher.Seal(buffer.Index(dataIndex), packetHeader[4:16], buffer.From(dataIndex), nil)
		buffer.Extend(shadowio.Overhead)
		c.method.udpBlockEncryptCipher.Encrypt(packetHeader, packetHeader)
	}
	err = common.Error(c.writer.Write(buffer.Bytes()))
	if err != nil {
		return
	}
	return len(p), nil
}

func (c *clientPacketConn) FrontHeadroom() int {
	var overHead int
	if c.method.udpCipher != nil {
		overHead = PacketNonceSize + shadowio.Overhead
	} else {
		overHead = shadowio.Overhead
	}
	overHead += 16 // packet header
	pskLen := len(c.method.pskList)
	if c.method.udpCipher == nil && pskLen > 1 {
		overHead += (pskLen - 1) * aes.BlockSize
	}
	overHead += 1 // header type
	overHead += 8 // timestamp
	overHead += 2 // padding length
	overHead += MaxPaddingLength
	overHead += M.MaxSocksaddrLength
	return overHead
}

func (c *clientPacketConn) RearHeadroom() int {
	return shadowio.Overhead
}

func (c *clientPacketConn) Upstream() any {
	return c.AbstractConn
}

func (c *clientPacketConn) Close() error {
	return c.AbstractConn.Close()
}
