package shadowaead_2022

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"encoding/binary"
	"io"
	"math"
	mRand "math/rand"
	"net"
	"os"
	"sync"
	"sync/atomic"
	"time"

	"github.com/sagernet/sing-shadowsocks"
	"github.com/sagernet/sing-shadowsocks/shadowaead"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/cache"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/replay"
	"github.com/sagernet/sing/common/udpnat"

	"golang.org/x/crypto/chacha20poly1305"
)

var (
	ErrInvalidRequest = E.New("invalid request")
	ErrNoPadding      = E.New("bad request: missing payload or padding")
	ErrBadPadding     = E.New("bad request: damaged padding")
)

var _ shadowsocks.Service = (*Service)(nil)

type Service struct {
	name          string
	keySaltLength int
	handler       shadowsocks.Handler
	timeFunc      func() time.Time

	constructor      func(key []byte) (cipher.AEAD, error)
	blockConstructor func(key []byte) (cipher.Block, error)
	udpCipher        cipher.AEAD
	udpBlockCipher   cipher.Block
	psk              []byte

	replayFilter replay.Filter
	udpNat       *udpnat.Service[uint64]
	udpSessions  *cache.LruCache[uint64, *serverUDPSession]
}

func NewServiceWithPassword(method string, password string, udpTimeout int64, handler shadowsocks.Handler, timeFunc func() time.Time) (shadowsocks.Service, error) {
	if password == "" {
		return nil, ErrMissingPSK
	}
	psk, err := base64.StdEncoding.DecodeString(password)
	if err != nil {
		return nil, E.Cause(err, "decode psk")
	}
	return NewService(method, psk, udpTimeout, handler, timeFunc)
}

func NewService(method string, psk []byte, udpTimeout int64, handler shadowsocks.Handler, timeFunc func() time.Time) (shadowsocks.Service, error) {
	s := &Service{
		name:     method,
		handler:  handler,
		timeFunc: timeFunc,

		replayFilter: replay.NewSimple(60 * time.Second),
		udpNat:       udpnat.New[uint64](udpTimeout, handler),
		udpSessions: cache.New[uint64, *serverUDPSession](
			cache.WithAge[uint64, *serverUDPSession](udpTimeout),
			cache.WithUpdateAgeOnGet[uint64, *serverUDPSession](),
		),
	}

	switch method {
	case "2022-blake3-aes-128-gcm":
		s.keySaltLength = 16
		s.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
		s.blockConstructor = aes.NewCipher
	case "2022-blake3-aes-256-gcm":
		s.keySaltLength = 32
		s.constructor = aeadCipher(aes.NewCipher, cipher.NewGCM)
		s.blockConstructor = aes.NewCipher
	case "2022-blake3-chacha20-poly1305":
		s.keySaltLength = 32
		s.constructor = chacha20poly1305.New
	default:
		return nil, os.ErrInvalid
	}

	if len(psk) != s.keySaltLength {
		if len(psk) < s.keySaltLength {
			return nil, shadowsocks.ErrBadKey
		} else if len(psk) > s.keySaltLength {
			psk = Key(psk, s.keySaltLength)
		} else {
			return nil, ErrMissingPSK
		}
	}

	var err error
	switch method {
	case "2022-blake3-aes-128-gcm", "2022-blake3-aes-256-gcm":
		s.udpBlockCipher, err = aes.NewCipher(psk)
	case "2022-blake3-chacha20-poly1305":
		s.udpCipher, err = chacha20poly1305.NewX(psk)
	}
	if err != nil {
		return nil, err
	}

	s.psk = psk
	return s, nil
}

func (s *Service) Name() string {
	return s.name
}

func (s *Service) Password() string {
	return base64.StdEncoding.EncodeToString(s.psk)
}

func (s *Service) NewConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	err := s.newConnection(ctx, conn, metadata)
	if err != nil {
		err = &shadowsocks.ServerConnError{Conn: conn, Source: metadata.Source, Cause: err}
	}
	return err
}

func (s *Service) time() time.Time {
	if s.timeFunc != nil {
		return s.timeFunc()
	} else {
		return time.Now()
	}
}

func (s *Service) newConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	header := make([]byte, s.keySaltLength+shadowaead.Overhead+RequestHeaderFixedChunkLength)

	n, err := conn.Read(header)
	if err != nil {
		return E.Cause(err, "read header")
	} else if n < len(header) {
		return shadowaead.ErrBadHeader
	}

	requestSalt := header[:s.keySaltLength]

	if !s.replayFilter.Check(requestSalt) {
		return ErrSaltNotUnique
	}

	requestKey := SessionKey(s.psk, requestSalt, s.keySaltLength)
	readCipher, err := s.constructor(requestKey)
	if err != nil {
		return err
	}
	reader := shadowaead.NewReader(
		conn,
		readCipher,
		MaxPacketSize,
	)

	err = reader.ReadExternalChunk(header[s.keySaltLength:])
	if err != nil {
		return err
	}

	headerType, err := reader.ReadByte()
	if err != nil {
		return E.Cause(err, "read header")
	}

	if headerType != HeaderTypeClient {
		return E.Extend(ErrBadHeaderType, "expected ", HeaderTypeClient, ", got ", headerType)
	}

	var epoch uint64
	err = binary.Read(reader, binary.BigEndian, &epoch)
	if err != nil {
		return err
	}

	diff := int(math.Abs(float64(s.time().Unix() - int64(epoch))))
	if diff > 30 {
		return E.Extend(ErrBadTimestamp, "received ", epoch, ", diff ", diff, "s")
	}

	var length uint16
	err = binary.Read(reader, binary.BigEndian, &length)
	if err != nil {
		return err
	}

	err = reader.ReadWithLength(length)
	if err != nil {
		return err
	}

	destination, err := M.SocksaddrSerializer.ReadAddrPort(reader)
	if err != nil {
		return err
	}

	var paddingLen uint16
	err = binary.Read(reader, binary.BigEndian, &paddingLen)
	if err != nil {
		return err
	}

	if uint16(reader.Cached()) < paddingLen {
		return ErrNoPadding
	}

	if paddingLen > 0 {
		err = reader.Discard(int(paddingLen))
		if err != nil {
			return E.Cause(err, "discard padding")
		}
	} else if reader.Cached() == 0 {
		return ErrNoPadding
	}

	protocolConn := &serverConn{
		Service:     s,
		Conn:        conn,
		uPSK:        s.psk,
		headerType:  headerType,
		requestSalt: requestSalt,
	}

	protocolConn.reader = reader

	metadata.Protocol = "shadowsocks"
	metadata.Destination = destination
	return s.handler.NewConnection(ctx, protocolConn, metadata)
}

type serverConn struct {
	*Service
	net.Conn
	uPSK        []byte
	access      sync.Mutex
	headerType  byte
	reader      *shadowaead.Reader
	writer      *shadowaead.Writer
	requestSalt []byte
}

func (c *serverConn) writeResponse(payload []byte) (n int, err error) {
	salt := buf.NewSize(c.keySaltLength)
	salt.WriteRandom(salt.FreeLen())

	key := SessionKey(c.uPSK, salt.Bytes(), c.keySaltLength)
	writeCipher, err := c.constructor(key)
	if err != nil {
		salt.Release()
		return
	}
	writer := shadowaead.NewWriter(
		c.Conn,
		writeCipher,
		MaxPacketSize,
	)
	header := writer.Buffer()
	header.Write(salt.Bytes())

	salt.Release()

	headerType := byte(HeaderTypeServer)
	payloadLen := len(payload)

	headerFixedChunk := buf.NewSize(1 + 8 + c.keySaltLength + 2)
	common.Must(headerFixedChunk.WriteByte(headerType))
	common.Must(binary.Write(headerFixedChunk, binary.BigEndian, uint64(c.time().Unix())))
	common.Must1(headerFixedChunk.Write(c.requestSalt))
	common.Must(binary.Write(headerFixedChunk, binary.BigEndian, uint16(payloadLen)))

	writer.WriteChunk(header, headerFixedChunk.Bytes())
	headerFixedChunk.Release()
	c.requestSalt = nil

	if payloadLen > 0 {
		writer.WriteChunk(header, payload[:payloadLen])
	}

	err = writer.BufferedWriter(header.Len()).Flush()
	if err != nil {
		return
	}

	switch headerType {
	case HeaderTypeServer:
		c.writer = writer
		// case HeaderTypeServerEncrypted:
		//	encryptedWriter := NewTLSEncryptedStreamWriter(writer)
		//	if payloadLen < len(payload) {
		//		_, err = encryptedWriter.Write(payload[payloadLen:])
		//		if err != nil {
		//			return
		//		}
		//	}
		//	c.writer = encryptedWriter
	}

	n = len(payload)
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

func (c *serverConn) WriteVectorised(buffers []*buf.Buffer) error {
	if c.writer != nil {
		return c.writer.WriteVectorised(buffers)
	}
	c.access.Lock()
	if c.writer != nil {
		c.access.Unlock()
		return c.writer.WriteVectorised(buffers)
	}
	defer c.access.Unlock()
	_, err := c.writeResponse(buffers[0].Bytes())
	if err != nil {
		buf.ReleaseMulti(buffers)
		return err
	}
	buffers[0].Release()
	return c.writer.WriteVectorised(buffers[1:])
}

func (c *serverConn) Close() error {
	return common.Close(
		c.Conn,
		common.PtrOrNil(c.reader),
		common.PtrOrNil(c.writer),
	)
}

func (c *serverConn) NeedAdditionalReadDeadline() bool {
	return true
}

func (c *serverConn) Upstream() any {
	return c.Conn
}

func (s *Service) WriteIsThreadUnsafe() {
}

func (s *Service) NewPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	err := s.newPacket(ctx, conn, buffer, metadata)
	if err != nil {
		err = &shadowsocks.ServerPacketError{Source: metadata.Source, Cause: err}
	}
	return err
}

func (s *Service) newPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	var packetHeader []byte
	if s.udpCipher != nil {
		if buffer.Len() < PacketNonceSize+PacketMinimalHeaderSize {
			return ErrPacketTooShort
		}
		_, err := s.udpCipher.Open(buffer.Index(PacketNonceSize), buffer.To(PacketNonceSize), buffer.From(PacketNonceSize), nil)
		if err != nil {
			return E.Cause(err, "decrypt packet header")
		}
		buffer.Advance(PacketNonceSize)
		buffer.Truncate(buffer.Len() - shadowaead.Overhead)
	} else {
		if buffer.Len() < PacketMinimalHeaderSize {
			return ErrPacketTooShort
		}
		packetHeader = buffer.To(aes.BlockSize)
		s.udpBlockCipher.Decrypt(packetHeader, packetHeader)
	}

	var sessionId, packetId uint64
	err := binary.Read(buffer, binary.BigEndian, &sessionId)
	if err != nil {
		return err
	}
	err = binary.Read(buffer, binary.BigEndian, &packetId)
	if err != nil {
		return err
	}

	session, loaded := s.udpSessions.LoadOrStore(sessionId, s.newUDPSession)
	if !loaded {
		session.remoteSessionId = sessionId
		if packetHeader != nil {
			key := SessionKey(s.psk, packetHeader[:8], s.keySaltLength)
			session.remoteCipher, err = s.constructor(key)
			if err != nil {
				return err
			}
		}
	}
	goto process

returnErr:
	if !loaded {
		s.udpSessions.Delete(sessionId)
	}
	return err

process:
	if !session.window.Check(packetId) {
		err = ErrPacketIdNotUnique
		goto returnErr
	}

	if packetHeader != nil {
		_, err = session.remoteCipher.Open(buffer.Index(0), packetHeader[4:16], buffer.Bytes(), nil)
		if err != nil {
			err = E.Cause(err, "decrypt packet")
			goto returnErr
		}
		buffer.Truncate(buffer.Len() - shadowaead.Overhead)
	}

	session.window.Add(packetId)

	var headerType byte
	headerType, err = buffer.ReadByte()
	if err != nil {
		err = E.Cause(err, "decrypt packet")
		goto returnErr
	}
	if headerType != HeaderTypeClient {
		err = E.Extend(ErrBadHeaderType, "expected ", HeaderTypeClient, ", got ", headerType)
		goto returnErr
	}

	var epoch uint64
	err = binary.Read(buffer, binary.BigEndian, &epoch)
	if err != nil {
		goto returnErr
	}
	diff := int(math.Abs(float64(s.time().Unix() - int64(epoch))))
	if diff > 30 {
		err = E.Extend(ErrBadTimestamp, "received ", epoch, ", diff ", diff, "s")
		goto returnErr
	}

	var paddingLen uint16
	err = binary.Read(buffer, binary.BigEndian, &paddingLen)
	if err != nil {
		err = E.Cause(err, "read padding length")
		goto returnErr
	}
	buffer.Advance(int(paddingLen))

	destination, err := M.SocksaddrSerializer.ReadAddrPort(buffer)
	if err != nil {
		goto returnErr
	}
	metadata.Protocol = "shadowsocks"
	metadata.Destination = destination
	s.udpNat.NewPacket(ctx, sessionId, buffer, metadata, func(natConn N.PacketConn) N.PacketWriter {
		return &serverPacketWriter{s, conn, natConn, session, s.udpBlockCipher}
	})
	return nil
}

func (s *Service) NewError(ctx context.Context, err error) {
	s.handler.NewError(ctx, err)
}

type serverPacketWriter struct {
	*Service
	source         N.PacketConn
	nat            N.PacketConn
	session        *serverUDPSession
	udpBlockCipher cipher.Block
}

func (w *serverPacketWriter) WritePacket(buffer *buf.Buffer, destination M.Socksaddr) error {
	var hdrLen int
	if w.udpCipher != nil {
		hdrLen = PacketNonceSize
	}

	var paddingLen int
	if destination.Port == 53 && buffer.Len() < MaxPaddingLength {
		paddingLen = mRand.Intn(MaxPaddingLength-buffer.Len()) + 1
	}

	hdrLen += 16 // packet header
	hdrLen += 1  // header type
	hdrLen += 8  // timestamp
	hdrLen += 8  // remote session id
	hdrLen += 2  // padding length
	hdrLen += paddingLen
	hdrLen += M.SocksaddrSerializer.AddrPortLen(destination)
	header := buf.With(buffer.ExtendHeader(hdrLen))

	var dataIndex int
	if w.udpCipher != nil {
		common.Must1(header.ReadFullFrom(w.session.rng, PacketNonceSize))
		dataIndex = PacketNonceSize
	} else {
		dataIndex = aes.BlockSize
	}

	common.Must(
		binary.Write(header, binary.BigEndian, w.session.sessionId),
		binary.Write(header, binary.BigEndian, w.session.nextPacketId()),
		header.WriteByte(HeaderTypeServer),
		binary.Write(header, binary.BigEndian, uint64(w.time().Unix())),
		binary.Write(header, binary.BigEndian, w.session.remoteSessionId),
		binary.Write(header, binary.BigEndian, uint16(paddingLen)), // padding length
	)

	if paddingLen > 0 {
		header.Extend(paddingLen)
	}

	err := M.SocksaddrSerializer.WriteAddrPort(header, destination)
	if err != nil {
		buffer.Release()
		return err
	}

	if w.udpCipher != nil {
		w.udpCipher.Seal(buffer.Index(dataIndex), buffer.To(dataIndex), buffer.From(dataIndex), nil)
		buffer.Extend(shadowaead.Overhead)
	} else {
		packetHeader := buffer.To(aes.BlockSize)
		w.session.cipher.Seal(buffer.Index(dataIndex), packetHeader[4:16], buffer.From(dataIndex), nil)
		buffer.Extend(shadowaead.Overhead)
		w.udpBlockCipher.Encrypt(packetHeader, packetHeader)
	}
	return w.source.WritePacket(buffer, M.SocksaddrFromNet(w.nat.LocalAddr()))
}

func (w *serverPacketWriter) FrontHeadroom() int {
	var hdrLen int
	if w.udpCipher != nil {
		hdrLen = PacketNonceSize
	}
	hdrLen += 16 // packet header
	hdrLen += 1  // header type
	hdrLen += 8  // timestamp
	hdrLen += 8  // remote session id
	hdrLen += 2  // padding length
	hdrLen += MaxPaddingLength
	hdrLen += M.MaxSocksaddrLength
	return hdrLen
}

func (w *serverPacketWriter) RearHeadroom() int {
	return shadowaead.Overhead
}

func (w *serverPacketWriter) Upstream() any {
	return w.source
}

type serverUDPSession struct {
	sessionId       uint64
	remoteSessionId uint64
	packetId        uint64
	cipher          cipher.AEAD
	remoteCipher    cipher.AEAD
	window          SlidingWindow
	rng             io.Reader
}

func (s *serverUDPSession) nextPacketId() uint64 {
	return atomic.AddUint64(&s.packetId, 1)
}

func (s *Service) newUDPSession() *serverUDPSession {
	session := &serverUDPSession{}
	if s.udpCipher != nil {
		session.rng = Blake3KeyedHash(rand.Reader)
		common.Must(binary.Read(session.rng, binary.BigEndian, &session.sessionId))
	} else {
		common.Must(binary.Read(rand.Reader, binary.BigEndian, &session.sessionId))
	}
	session.packetId--
	if s.udpCipher == nil {
		sessionId := make([]byte, 8)
		binary.BigEndian.PutUint64(sessionId, session.sessionId)
		key := SessionKey(s.psk, sessionId, s.keySaltLength)
		var err error
		session.cipher, err = s.constructor(key)
		common.Must(err)
	}
	return session
}
