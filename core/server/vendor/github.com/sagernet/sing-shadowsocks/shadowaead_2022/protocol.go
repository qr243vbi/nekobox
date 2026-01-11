package shadowaead_2022

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/binary"
	"io"
	"math"
	mRand "math/rand"
	"net"
	"os"
	"strings"
	"sync/atomic"
	"time"

	"github.com/sagernet/sing-shadowsocks"
	"github.com/sagernet/sing-shadowsocks/shadowaead"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/random"
	"github.com/sagernet/sing/common/rw"

	"golang.org/x/crypto/chacha20poly1305"
	"lukechampine.com/blake3"
)

const (
	HeaderTypeClient              = 0
	HeaderTypeServer              = 1
	MaxPaddingLength              = 900
	PacketNonceSize               = 24
	MaxPacketSize                 = 65535
	RequestHeaderFixedChunkLength = 1 + 8 + 2
	PacketMinimalHeaderSize       = 30
)

var (
	ErrMissingPSK            = E.New("missing psk")
	ErrBadHeaderType         = E.New("bad header type")
	ErrBadTimestamp          = E.New("bad timestamp")
	ErrBadRequestSalt        = E.New("bad request salt")
	ErrSaltNotUnique         = E.New("salt not unique")
	ErrBadClientSessionId    = E.New("bad client session id")
	ErrPacketIdNotUnique     = E.New("packet id not unique")
	ErrTooManyServerSessions = E.New("server session changed more than once during the last minute")
	ErrPacketTooShort        = E.New("packet too short")
)

var List = []string{
	"2022-blake3-aes-128-gcm",
	"2022-blake3-aes-256-gcm",
	"2022-blake3-chacha20-poly1305",
}

func init() {
	random.InitializeSeed()
}

func NewWithPassword(method string, password string, timeFunc func() time.Time) (shadowsocks.Method, error) {
	var pskList [][]byte
	if password == "" {
		return nil, ErrMissingPSK
	}
	keyStrList := strings.Split(password, ":")
	pskList = make([][]byte, len(keyStrList))
	for i, keyStr := range keyStrList {
		kb, err := base64.StdEncoding.DecodeString(keyStr)
		if err != nil {
			return nil, E.Cause(err, "decode key")
		}
		pskList[i] = kb
	}
	return New(method, pskList, timeFunc)
}

func New(method string, pskList [][]byte, timeFunc func() time.Time) (shadowsocks.Method, error) {
	m := &Method{
		name:     method,
		timeFunc: timeFunc,
	}

	switch method {
	case "2022-blake3-aes-128-gcm":
		m.keySaltLength = 16
		m.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
		m.blockConstructor = aes.NewCipher
	case "2022-blake3-aes-256-gcm":
		m.keySaltLength = 32
		m.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
		m.blockConstructor = aes.NewCipher
	case "2022-blake3-chacha20-poly1305":
		if len(pskList) > 1 {
			return nil, os.ErrInvalid
		}
		m.keySaltLength = 32
		m.constructor = chacha20poly1305.New
	}

	if len(pskList) == 0 {
		return nil, ErrMissingPSK
	}

	for i, psk := range pskList {
		if len(psk) < m.keySaltLength {
			return nil, shadowsocks.ErrBadKey
		} else if len(psk) > m.keySaltLength {
			pskList[i] = Key(psk, m.keySaltLength)
		}
	}

	if len(pskList) > 1 {
		pskHash := make([]byte, (len(pskList)-1)*aes.BlockSize)
		for i, psk := range pskList {
			if i == 0 {
				continue
			}
			hash := blake3.Sum512(psk)
			copy(pskHash[aes.BlockSize*(i-1):aes.BlockSize*i], hash[:aes.BlockSize])
		}
		m.pskHash = pskHash
	}

	var err error
	switch method {
	case "2022-blake3-aes-128-gcm", "2022-blake3-aes-256-gcm":
		m.udpBlockEncryptCipher, err = aes.NewCipher(pskList[0])
		if err != nil {
			return nil, err
		}
		m.udpBlockDecryptCipher, err = aes.NewCipher(pskList[len(pskList)-1])
		if err != nil {
			return nil, err
		}
	case "2022-blake3-chacha20-poly1305":
		m.udpCipher, err = chacha20poly1305.NewX(pskList[0])
		if err != nil {
			return nil, err
		}
	}

	m.pskList = pskList
	return m, nil
}

func Key(key []byte, keyLength int) []byte {
	psk := sha256.Sum256(key)
	return psk[:keyLength]
}

func SessionKey(psk []byte, salt []byte, keyLength int) []byte {
	sessionKey := make([]byte, len(psk)+len(salt))
	copy(sessionKey, psk)
	copy(sessionKey[len(psk):], salt)
	outKey := make([]byte, keyLength)
	blake3.DeriveKey(outKey, "shadowsocks 2022 session subkey", sessionKey)
	return outKey
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
	timeFunc      func() time.Time

	constructor           func(key []byte) (cipher.AEAD, error)
	blockConstructor      func(key []byte) (cipher.Block, error)
	udpCipher             cipher.AEAD
	udpBlockEncryptCipher cipher.Block
	udpBlockDecryptCipher cipher.Block
	pskList               [][]byte
	pskHash               []byte
}

func (m *Method) Name() string {
	return m.name
}

func (m *Method) DialConn(conn net.Conn, destination M.Socksaddr) (net.Conn, error) {
	shadowsocksConn := &clientConn{
		Method:      m,
		Conn:        conn,
		destination: destination,
	}
	return shadowsocksConn, shadowsocksConn.writeRequest(nil)
}

func (m *Method) DialEarlyConn(conn net.Conn, destination M.Socksaddr) net.Conn {
	return &clientConn{
		Method:      m,
		Conn:        conn,
		destination: destination,
	}
}

func (m *Method) DialPacketConn(conn net.Conn) N.NetPacketConn {
	return &clientPacketConn{m, conn, m.newUDPSession()}
}

type clientConn struct {
	*Method
	net.Conn
	destination M.Socksaddr
	requestSalt []byte
	reader      *shadowaead.Reader
	writer      *shadowaead.Writer
}

func (m *Method) time() time.Time {
	if m.timeFunc != nil {
		return m.timeFunc()
	} else {
		return time.Now()
	}
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
		identitySubkey := buf.NewSize(m.keySaltLength)
		identitySubkey.Extend(identitySubkey.FreeLen())
		blake3.DeriveKey(identitySubkey.Bytes(), "shadowsocks 2022 identity subkey", keyMaterial)

		pskHash := m.pskHash[aes.BlockSize*i : aes.BlockSize*(i+1)]

		header := request.Extend(16)
		b, err := m.blockConstructor(identitySubkey.Bytes())
		if err != nil {
			return err
		}
		b.Encrypt(header, pskHash)
		identitySubkey.Release()
		if i == pskLen-2 {
			break
		}
	}
	return nil
}

func (c *clientConn) writeRequest(payload []byte) error {
	salt := make([]byte, c.keySaltLength)
	common.Must1(io.ReadFull(rand.Reader, salt))

	key := SessionKey(c.pskList[len(c.pskList)-1], salt, c.keySaltLength)
	writeCipher, err := c.constructor(key)
	if err != nil {
		return err
	}
	writer := shadowaead.NewWriter(
		c.Conn,
		writeCipher,
		MaxPacketSize,
	)

	header := writer.Buffer()
	header.Write(salt)

	err = c.writeExtendedIdentityHeaders(header, salt)
	if err != nil {
		return err
	}

	var _fixedLengthBuffer [RequestHeaderFixedChunkLength]byte
	fixedLengthBuffer := buf.With(_fixedLengthBuffer[:])
	common.Must(fixedLengthBuffer.WriteByte(HeaderTypeClient))
	common.Must(binary.Write(fixedLengthBuffer, binary.BigEndian, uint64(c.time().Unix())))
	var paddingLen int
	if len(payload) < MaxPaddingLength {
		paddingLen = mRand.Intn(MaxPaddingLength) + 1
	}
	variableLengthHeaderLen := M.SocksaddrSerializer.AddrPortLen(c.destination) + 2 + paddingLen
	payloadLen := len(payload)
	variableLengthHeaderLen += payloadLen
	common.Must(binary.Write(fixedLengthBuffer, binary.BigEndian, uint16(variableLengthHeaderLen)))
	writer.WriteChunk(header, fixedLengthBuffer.Bytes())

	variableLengthBuffer := buf.NewSize(variableLengthHeaderLen)
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
	writer.WriteChunk(header, variableLengthBuffer.Bytes())
	variableLengthBuffer.Release()

	err = writer.BufferedWriter(header.Len()).Flush()
	if err != nil {
		return E.Cause(err, "client handshake")
	}

	c.requestSalt = salt
	c.writer = writer
	return nil
}

func (c *clientConn) readResponse() error {
	if c.reader != nil {
		return nil
	}

	salt := buf.NewSize(c.keySaltLength)

	_, err := salt.ReadFullFrom(c.Conn, salt.FreeLen())
	if err != nil {
		salt.Release()
		return err
	}

	key := SessionKey(c.pskList[len(c.pskList)-1], salt.Bytes(), c.keySaltLength)
	salt.Release()

	readCipher, err := c.constructor(key)
	if err != nil {
		return err
	}
	reader := shadowaead.NewReader(
		c.Conn,
		readCipher,
		MaxPacketSize,
	)

	err = reader.ReadWithLength(uint16(1 + 8 + c.keySaltLength + 2))
	if err != nil {
		return E.Cause(err, "read response fixed length chunk")
	}

	headerType, err := rw.ReadByte(reader)
	if err != nil {
		return err
	}
	if headerType != HeaderTypeServer /* && headerType != HeaderTypeServerEncrypted*/ {
		return E.Extend(ErrBadHeaderType, "expected ", HeaderTypeServer, ", got ", headerType)
	}

	var epoch uint64
	err = binary.Read(reader, binary.BigEndian, &epoch)
	if err != nil {
		return err
	}

	diff := int(math.Abs(float64(c.time().Unix() - int64(epoch))))
	if diff > 30 {
		return E.Extend(ErrBadTimestamp, "received ", epoch, ", diff ", diff, "s")
	}

	requestSalt := buf.NewSize(c.keySaltLength)
	_, err = requestSalt.ReadFullFrom(reader, requestSalt.FreeLen())
	if err != nil {
		return err
	}

	if bytes.Compare(requestSalt.Bytes(), c.requestSalt) > 0 {
		return ErrBadRequestSalt
	}
	requestSalt.Release()
	c.requestSalt = nil

	var length uint16
	err = binary.Read(reader, binary.BigEndian, &length)
	if err != nil {
		return err
	}

	err = reader.ReadWithLength(length)
	if err != nil {
		return err
	}
	if headerType == HeaderTypeServer {
		c.reader = reader
	}
	return nil
}

func (c *clientConn) Read(p []byte) (n int, err error) {
	if err = c.readResponse(); err != nil {
		return
	}
	return c.reader.Read(p)
}

func (c *clientConn) WriteTo(w io.Writer) (n int64, err error) {
	if err = c.readResponse(); err != nil {
		return
	}
	return bufio.Copy(w, c.reader)
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

var _ N.VectorisedWriter = (*clientConn)(nil)

func (c *clientConn) WriteVectorised(buffers []*buf.Buffer) error {
	if c.writer != nil {
		return c.writer.WriteVectorised(buffers)
	}
	err := c.writeRequest(buffers[0].Bytes())
	if err != nil {
		buf.ReleaseMulti(buffers)
		return err
	}
	buffers[0].Release()
	return c.writer.WriteVectorised(buffers[1:])
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

func (c *clientConn) Close() error {
	return common.Close(
		c.Conn,
		common.PtrOrNil(c.reader),
		common.PtrOrNil(c.writer),
	)
}

type clientPacketConn struct {
	*Method
	net.Conn
	session *udpSession
}

func (c *clientPacketConn) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	defer buffer.Release()
	var hdrLen int
	if c.udpCipher != nil {
		hdrLen = PacketNonceSize
	}

	var paddingLen int
	if destination.Port == 53 && buffer.Len() < MaxPaddingLength {
		paddingLen = mRand.Intn(MaxPaddingLength-buffer.Len()) + 1
	}

	hdrLen += 16 // packet header
	pskLen := len(c.pskList)
	if c.udpCipher == nil && pskLen > 1 {
		hdrLen += (pskLen - 1) * aes.BlockSize
	}
	hdrLen += 1 // header type
	hdrLen += 8 // timestamp
	hdrLen += 2 // padding length
	hdrLen += paddingLen
	hdrLen += M.SocksaddrSerializer.AddrPortLen(destination)
	header := buf.With(buffer.ExtendHeader(hdrLen))

	var dataIndex int
	if c.udpCipher != nil {
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

	if c.udpCipher == nil && pskLen > 1 {
		for i, psk := range c.pskList {
			dataIndex += aes.BlockSize
			pskHash := c.pskHash[aes.BlockSize*i : aes.BlockSize*(i+1)]

			identityHeader := header.Extend(aes.BlockSize)
			xorWords(identityHeader, pskHash, header.To(aes.BlockSize))
			b, err := c.blockConstructor(psk)
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
		binary.Write(header, binary.BigEndian, uint64(c.time().Unix())),
		binary.Write(header, binary.BigEndian, uint16(paddingLen)), // padding length
	)

	if paddingLen > 0 {
		header.Extend(paddingLen)
	}

	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		return err
	}
	if c.udpCipher != nil {
		c.udpCipher.Seal(buffer.Index(dataIndex), buffer.To(dataIndex), buffer.From(dataIndex), nil)
		buffer.Extend(shadowaead.Overhead)
	} else {
		packetHeader := buffer.To(aes.BlockSize)
		c.session.cipher.Seal(buffer.Index(dataIndex), packetHeader[4:16], buffer.From(dataIndex), nil)
		buffer.Extend(shadowaead.Overhead)
		c.udpBlockEncryptCipher.Encrypt(packetHeader, packetHeader)
	}
	return common.Error(c.Write(buffer.Bytes()))
}

func (c *clientPacketConn) ReadPacket(buffer *buf.Buffer) (M.Socksaddr, error) {
	n, err := c.Read(buffer.FreeBytes())
	if err != nil {
		return M.Socksaddr{}, err
	}
	buffer.Truncate(n)

	var packetHeader []byte
	if c.udpCipher != nil {
		if buffer.Len() < PacketNonceSize+PacketMinimalHeaderSize {
			return M.Socksaddr{}, ErrPacketTooShort
		}
		_, err = c.udpCipher.Open(buffer.Index(PacketNonceSize), buffer.To(PacketNonceSize), buffer.From(PacketNonceSize), nil)
		if err != nil {
			return M.Socksaddr{}, E.Cause(err, "decrypt packet")
		}
		buffer.Advance(PacketNonceSize)
		buffer.Truncate(buffer.Len() - shadowaead.Overhead)
	} else {
		if buffer.Len() < PacketMinimalHeaderSize {
			return M.Socksaddr{}, ErrPacketTooShort
		}
		packetHeader = buffer.To(aes.BlockSize)
		c.udpBlockDecryptCipher.Decrypt(packetHeader, packetHeader)
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
			key := SessionKey(c.pskList[len(c.pskList)-1], packetHeader[:8], c.keySaltLength)
			remoteCipher, err = c.constructor(key)
			if err != nil {
				return M.Socksaddr{}, err
			}
		}
		_, err = remoteCipher.Open(buffer.Index(0), packetHeader[4:16], buffer.Bytes(), nil)
		if err != nil {
			return M.Socksaddr{}, E.Cause(err, "decrypt packet")
		}
		buffer.Truncate(buffer.Len() - shadowaead.Overhead)
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

	diff := int(math.Abs(float64(c.time().Unix() - int64(epoch))))
	if diff > 30 {
		return M.Socksaddr{}, E.Extend(ErrBadTimestamp, "received ", epoch, ", diff ", diff, "s")
	}

	if sessionId == c.session.remoteSessionId {
		c.session.window.Add(packetId)
	} else if sessionId == c.session.lastRemoteSessionId {
		c.session.lastWindow.Add(packetId)
		c.session.lastRemoteSeen = c.time().Unix()
	} else {
		if c.session.remoteSessionId != 0 {
			if c.time().Unix()-c.session.lastRemoteSeen < 60 {
				return M.Socksaddr{}, ErrTooManyServerSessions
			} else {
				c.session.lastRemoteSessionId = c.session.remoteSessionId
				c.session.lastWindow = c.session.window
				c.session.lastRemoteSeen = c.time().Unix()
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

	destination, err := M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		return M.Socksaddr{}, err
	}
	return destination, nil
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
	var overHead int
	if c.udpCipher != nil {
		overHead = PacketNonceSize + shadowaead.Overhead
	} else {
		overHead = shadowaead.Overhead
	}
	overHead += 16 // packet header
	pskLen := len(c.pskList)
	if c.udpCipher == nil && pskLen > 1 {
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
	if c.udpCipher != nil {
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

	if c.udpCipher == nil && pskLen > 1 {
		for i, psk := range c.pskList {
			dataIndex += aes.BlockSize
			pskHash := c.pskHash[aes.BlockSize*i : aes.BlockSize*(i+1)]

			identityHeader := buffer.Extend(aes.BlockSize)
			xorWords(identityHeader, pskHash, buffer.To(aes.BlockSize))
			b, err := c.blockConstructor(psk)
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
		binary.Write(buffer, binary.BigEndian, uint64(c.time().Unix())),
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
	if c.udpCipher != nil {
		c.udpCipher.Seal(buffer.Index(dataIndex), buffer.To(dataIndex), buffer.From(dataIndex), nil)
		buffer.Extend(shadowaead.Overhead)
	} else {
		packetHeader := buffer.To(aes.BlockSize)
		c.session.cipher.Seal(buffer.Index(dataIndex), packetHeader[4:16], buffer.From(dataIndex), nil)
		buffer.Extend(shadowaead.Overhead)
		c.udpBlockEncryptCipher.Encrypt(packetHeader, packetHeader)
	}
	err = common.Error(c.Write(buffer.Bytes()))
	if err != nil {
		return
	}
	return len(p), nil
}

func (c *clientPacketConn) FrontHeadroom() int {
	var overHead int
	if c.udpCipher != nil {
		overHead = PacketNonceSize + shadowaead.Overhead
	} else {
		overHead = shadowaead.Overhead
	}
	overHead += 16 // packet header
	pskLen := len(c.pskList)
	if c.udpCipher == nil && pskLen > 1 {
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
	return shadowaead.Overhead
}

type udpSession struct {
	sessionId           uint64
	packetId            uint64
	remoteSessionId     uint64
	lastRemoteSessionId uint64
	lastRemoteSeen      int64
	cipher              cipher.AEAD
	remoteCipher        cipher.AEAD
	lastRemoteCipher    cipher.AEAD
	window              SlidingWindow
	lastWindow          SlidingWindow
	rng                 io.Reader
}

func (s *udpSession) nextPacketId() uint64 {
	return atomic.AddUint64(&s.packetId, 1)
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

func (c *clientPacketConn) Upstream() any {
	return c.Conn
}

func (c *clientPacketConn) Close() error {
	return common.Close(c.Conn)
}

func Blake3KeyedHash(reader io.Reader) io.Reader {
	key := make([]byte, 32)
	common.Must1(io.ReadFull(reader, key))
	h := blake3.New(1024, key)
	return h.XOF()
}
