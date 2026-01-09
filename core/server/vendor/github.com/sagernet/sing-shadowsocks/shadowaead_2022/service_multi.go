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
	"net"
	"os"
	"time"

	"github.com/sagernet/sing-shadowsocks"
	"github.com/sagernet/sing-shadowsocks/shadowaead"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/auth"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/rw"

	"lukechampine.com/blake3"
)

var _ shadowsocks.MultiService[int] = (*MultiService[int])(nil)

type MultiService[U comparable] struct {
	*Service

	uPSK     map[U][]byte
	uPSKHash map[[aes.BlockSize]byte]U
	uCipher  map[U]cipher.Block
}

func NewMultiServiceWithPassword[U comparable](method string, password string, udpTimeout int64, handler shadowsocks.Handler, timeFunc func() time.Time) (*MultiService[U], error) {
	if password == "" {
		return nil, ErrMissingPSK
	}
	iPSK, err := base64.StdEncoding.DecodeString(password)
	if err != nil {
		return nil, E.Cause(err, "decode psk")
	}
	return NewMultiService[U](method, iPSK, udpTimeout, handler, timeFunc)
}

func NewMultiService[U comparable](method string, iPSK []byte, udpTimeout int64, handler shadowsocks.Handler, timeFunc func() time.Time) (*MultiService[U], error) {
	switch method {
	case "2022-blake3-aes-128-gcm":
	case "2022-blake3-aes-256-gcm":
	default:
		return nil, os.ErrInvalid
	}

	ss, err := NewService(method, iPSK, udpTimeout, handler, timeFunc)
	if err != nil {
		return nil, err
	}

	s := &MultiService[U]{
		Service: ss.(*Service),

		uPSK:     make(map[U][]byte),
		uPSKHash: make(map[[aes.BlockSize]byte]U),
	}
	return s, nil
}

func (s *MultiService[U]) UpdateUsers(userList []U, keyList [][]byte) error {
	uPSK := make(map[U][]byte)
	uPSKHash := make(map[[aes.BlockSize]byte]U)
	uCipher := make(map[U]cipher.Block)
	for i, user := range userList {
		key := keyList[i]
		if len(key) < s.keySaltLength {
			return shadowsocks.ErrBadKey
		} else if len(key) > s.keySaltLength {
			key = Key(key, s.keySaltLength)
		}

		var hash [aes.BlockSize]byte
		hash512 := blake3.Sum512(key)
		copy(hash[:], hash512[:])

		uPSKHash[hash] = user
		uPSK[user] = key
		var err error
		uCipher[user], err = s.blockConstructor(key)
		if err != nil {
			return err
		}
	}

	s.uPSK = uPSK
	s.uPSKHash = uPSKHash
	s.uCipher = uCipher
	return nil
}

func (s *MultiService[U]) UpdateUsersWithPasswords(userList []U, passwordList []string) error {
	keyList := make([][]byte, 0, len(passwordList))
	for _, password := range passwordList {
		if password == "" {
			return shadowsocks.ErrMissingPassword
		}
		uPSK, err := base64.StdEncoding.DecodeString(password)
		if err != nil {
			return E.Cause(err, "decode psk")
		}
		keyList = append(keyList, uPSK)
	}
	return s.UpdateUsers(userList, keyList)
}

func (s *MultiService[U]) NewConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	err := s.NewConnection0(ctx, conn, metadata, conn, nil)
	if err != nil {
		err = &shadowsocks.ServerConnError{Conn: conn, Source: metadata.Source, Cause: err}
	}
	return err
}

func (s *MultiService[U]) NewConnection0(ctx context.Context, conn net.Conn, metadata M.Metadata, handshakeReader io.Reader, handshakeSuccess func()) error {
	requestHeader := make([]byte, s.keySaltLength+aes.BlockSize+shadowaead.Overhead+RequestHeaderFixedChunkLength)
	var (
		n   int
		err error
	)
	if handshakeSuccess != nil {
		n, err = io.ReadFull(handshakeReader, requestHeader)
	} else {
		n, err = handshakeReader.Read(requestHeader)
	}
	if err != nil {
		return err
	} else if n < len(requestHeader) {
		return shadowaead.ErrBadHeader
	}
	requestSalt := requestHeader[:s.keySaltLength]
	if !s.replayFilter.Check(requestSalt) {
		return ErrSaltNotUnique
	}

	var _eiHeader [aes.BlockSize]byte
	eiHeader := _eiHeader[:]
	copy(eiHeader, requestHeader[s.keySaltLength:s.keySaltLength+aes.BlockSize])

	keyMaterial := make([]byte, s.keySaltLength*2)
	copy(keyMaterial, s.psk)
	copy(keyMaterial[s.keySaltLength:], requestSalt)
	identitySubkey := buf.NewSize(s.keySaltLength)
	identitySubkey.Extend(identitySubkey.FreeLen())
	blake3.DeriveKey(identitySubkey.Bytes(), "shadowsocks 2022 identity subkey", keyMaterial)
	b, err := s.blockConstructor(identitySubkey.Bytes())
	identitySubkey.Release()
	if err != nil {
		return err
	}
	b.Decrypt(eiHeader, eiHeader)

	var user U
	var uPSK []byte
	if u, loaded := s.uPSKHash[_eiHeader]; loaded {
		user = u
		uPSK = s.uPSK[u]
	} else {
		return ErrInvalidRequest
	}

	if handshakeSuccess != nil {
		handshakeSuccess()
	}

	requestKey := SessionKey(uPSK, requestSalt, s.keySaltLength)
	readCipher, err := s.constructor(requestKey)
	if err != nil {
		return err
	}
	reader := shadowaead.NewReader(
		conn,
		readCipher,
		MaxPacketSize,
	)

	err = reader.ReadExternalChunk(requestHeader[s.keySaltLength+aes.BlockSize:])
	if err != nil {
		return err
	}

	headerType, err := rw.ReadByte(reader)
	if err != nil {
		return E.Cause(err, "read header")
	}

	if headerType != HeaderTypeClient {
		return E.Extend(ErrBadHeaderType, "expected ", HeaderTypeClient, ", got ", headerType)
	}

	var epoch uint64
	err = binary.Read(reader, binary.BigEndian, &epoch)
	if err != nil {
		return E.Cause(err, "read timestamp")
	}
	diff := int(math.Abs(float64(s.time().Unix() - int64(epoch))))
	if diff > 30 {
		return E.Extend(ErrBadTimestamp, "received ", epoch, ", diff ", diff, "s")
	}
	var length uint16
	err = binary.Read(reader, binary.BigEndian, &length)
	if err != nil {
		return E.Cause(err, "read length")
	}

	err = reader.ReadWithLength(length)
	if err != nil {
		return err
	}

	destination, err := M.SocksaddrSerializer.ReadAddrPort(reader)
	if err != nil {
		return E.Cause(err, "read destination")
	}

	var paddingLen uint16
	err = binary.Read(reader, binary.BigEndian, &paddingLen)
	if err != nil {
		return E.Cause(err, "read padding length")
	}

	if reader.Cached() < int(paddingLen) {
		return ErrBadPadding
	} else if paddingLen > 0 {
		err = reader.Discard(int(paddingLen))
		if err != nil {
			return E.Cause(err, "discard padding")
		}
	} else if reader.Cached() == 0 {
		return ErrNoPadding
	}

	protocolConn := &serverConn{
		Service:     s.Service,
		Conn:        conn,
		uPSK:        uPSK,
		headerType:  headerType,
		requestSalt: requestSalt,
	}

	protocolConn.reader = reader
	metadata.Protocol = "shadowsocks"
	metadata.Destination = destination
	return s.handler.NewConnection(auth.ContextWithUser(ctx, user), protocolConn, metadata)
}

func (s *MultiService[U]) WriteIsThreadUnsafe() {
}

func (s *MultiService[U]) NewPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	err := s.newPacket(ctx, conn, buffer, metadata)
	if err != nil {
		err = &shadowsocks.ServerPacketError{Source: metadata.Source, Cause: err}
	}
	return err
}

func (s *MultiService[U]) newPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	if buffer.Len() < PacketMinimalHeaderSize {
		return ErrPacketTooShort
	}

	packetHeader := buffer.To(aes.BlockSize)
	s.udpBlockCipher.Decrypt(packetHeader, packetHeader)

	var _eiHeader [aes.BlockSize]byte
	eiHeader := _eiHeader[:]
	s.udpBlockCipher.Decrypt(eiHeader, buffer.Range(aes.BlockSize, 2*aes.BlockSize))
	xorWords(eiHeader, eiHeader, packetHeader)

	var user U
	var uPSK []byte
	if u, loaded := s.uPSKHash[_eiHeader]; loaded {
		user = u
		uPSK = s.uPSK[u]
	} else {
		return E.New("invalid request")
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

	buffer.Advance(aes.BlockSize)

	session, loaded := s.udpSessions.LoadOrStore(sessionId, func() *serverUDPSession {
		return s.newUDPSession(uPSK)
	})
	if !loaded {
		session.remoteSessionId = sessionId
		key := SessionKey(uPSK, packetHeader[:8], s.keySaltLength)
		session.remoteCipher, err = s.constructor(key)
		if err != nil {
			return err
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
	s.udpNat.NewContextPacket(ctx, sessionId, buffer, metadata, func(natConn N.PacketConn) (context.Context, N.PacketWriter) {
		return auth.ContextWithUser(ctx, user), &serverPacketWriter{s.Service, conn, natConn, session, s.uCipher[user]}
	})
	return nil
}

func (s *MultiService[U]) newUDPSession(uPSK []byte) *serverUDPSession {
	session := &serverUDPSession{}
	if s.udpCipher != nil {
		session.rng = Blake3KeyedHash(rand.Reader)
		common.Must(binary.Read(session.rng, binary.BigEndian, &session.sessionId))
	} else {
		common.Must(binary.Read(rand.Reader, binary.BigEndian, &session.sessionId))
	}
	session.packetId--
	sessionId := make([]byte, 8)
	binary.BigEndian.PutUint64(sessionId, session.sessionId)
	key := SessionKey(uPSK, sessionId, s.keySaltLength)
	var err error
	session.cipher, err = s.constructor(key)
	common.Must(err)
	return session
}
