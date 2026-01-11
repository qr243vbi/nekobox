package shadowaead_2022

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"encoding/base64"
	"encoding/binary"
	"net"
	"os"

	"github.com/sagernet/sing-shadowsocks"
	"github.com/sagernet/sing-shadowsocks/shadowaead"
	"github.com/sagernet/sing/common/auth"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/common/udpnat"

	"lukechampine.com/blake3"
)

var _ shadowsocks.Service = (*RelayService[int])(nil)

type RelayService[U comparable] struct {
	name          string
	keySaltLength int
	handler       shadowsocks.Handler

	constructor      func(key []byte) (cipher.AEAD, error)
	blockConstructor func(key []byte) (cipher.Block, error)
	udpBlockCipher   cipher.Block

	iPSK         []byte
	uPSKHash     map[[aes.BlockSize]byte]U
	uDestination map[U]M.Socksaddr
	uCipher      map[U]cipher.Block
	udpNat       *udpnat.Service[uint64]
}

func (s *RelayService[U]) Name() string {
	return s.name
}

func (s *RelayService[U]) Password() string {
	return base64.StdEncoding.EncodeToString(s.iPSK)
}

func (s *RelayService[U]) UpdateUsers(userList []U, keyList [][]byte, destinationList []M.Socksaddr) error {
	uPSKHash := make(map[[aes.BlockSize]byte]U)
	uDestination := make(map[U]M.Socksaddr)
	uCipher := make(map[U]cipher.Block)
	for i, user := range userList {
		key := keyList[i]
		destination := destinationList[i]
		if len(key) < s.keySaltLength {
			return shadowsocks.ErrBadKey
		} else if len(key) > s.keySaltLength {
			key = Key(key, s.keySaltLength)
		}

		var hash [aes.BlockSize]byte
		hash512 := blake3.Sum512(key)
		copy(hash[:], hash512[:])

		uPSKHash[hash] = user
		uDestination[user] = destination
		var err error
		uCipher[user], err = s.blockConstructor(key)
		if err != nil {
			return err
		}
	}

	s.uPSKHash = uPSKHash
	s.uDestination = uDestination
	s.uCipher = uCipher
	return nil
}

func (s *RelayService[U]) UpdateUsersWithPasswords(userList []U, passwordList []string, destinationList []M.Socksaddr) error {
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
	return s.UpdateUsers(userList, keyList, destinationList)
}

func NewRelayServiceWithPassword[U comparable](method string, password string, udpTimeout int64, handler shadowsocks.Handler) (*RelayService[U], error) {
	if password == "" {
		return nil, ErrMissingPSK
	}
	iPSK, err := base64.StdEncoding.DecodeString(password)
	if err != nil {
		return nil, E.Cause(err, "decode psk")
	}
	return NewRelayService[U](method, iPSK, udpTimeout, handler)
}

func NewRelayService[U comparable](method string, psk []byte, udpTimeout int64, handler shadowsocks.Handler) (*RelayService[U], error) {
	s := &RelayService[U]{
		name:    method,
		handler: handler,

		uPSKHash:     make(map[[aes.BlockSize]byte]U),
		uDestination: make(map[U]M.Socksaddr),
		uCipher:      make(map[U]cipher.Block),

		udpNat: udpnat.New[uint64](udpTimeout, handler),
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
	default:
		return nil, os.ErrInvalid
	}
	if len(psk) != s.keySaltLength {
		if len(psk) < s.keySaltLength {
			return nil, shadowsocks.ErrBadKey
		} else {
			psk = Key(psk, s.keySaltLength)
		}
	}
	s.iPSK = psk
	var err error
	s.udpBlockCipher, err = s.blockConstructor(psk)
	return s, err
}

func (s *RelayService[U]) NewConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	err := s.newConnection(ctx, conn, metadata)
	if err != nil {
		err = &shadowsocks.ServerConnError{Conn: conn, Source: metadata.Source, Cause: err}
	}
	return err
}

func (s *RelayService[U]) newConnection(ctx context.Context, conn net.Conn, metadata M.Metadata) error {
	requestHeader := buf.New()
	defer requestHeader.Release()
	n, err := requestHeader.ReadOnceFrom(conn)
	if err != nil {
		return err
	} else if int(n) < s.keySaltLength+aes.BlockSize {
		return shadowaead.ErrBadHeader
	}
	requestSalt := requestHeader.To(s.keySaltLength)
	var _eiHeader [aes.BlockSize]byte
	eiHeader := _eiHeader[:]
	copy(eiHeader, requestHeader.Range(s.keySaltLength, s.keySaltLength+aes.BlockSize))

	keyMaterial := make([]byte, s.keySaltLength*2)
	copy(keyMaterial, s.iPSK)
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
	if u, loaded := s.uPSKHash[_eiHeader]; loaded {
		user = u
	} else {
		return E.New("invalid request")
	}

	copy(requestHeader.Range(aes.BlockSize, aes.BlockSize+s.keySaltLength), requestHeader.To(s.keySaltLength))
	requestHeader.Advance(aes.BlockSize)

	metadata.Protocol = "shadowsocks-relay"
	metadata.Destination = s.uDestination[user]
	conn = bufio.NewCachedConn(conn, requestHeader)
	return s.handler.NewConnection(auth.ContextWithUser(ctx, user), conn, metadata)
}

func (s *RelayService[U]) WriteIsThreadUnsafe() {
}

func (s *RelayService[U]) NewPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	err := s.newPacket(ctx, conn, buffer, metadata)
	if err != nil {
		err = &shadowsocks.ServerPacketError{Source: metadata.Source, Cause: err}
	}
	return err
}

func (s *RelayService[U]) newPacket(ctx context.Context, conn N.PacketConn, buffer *buf.Buffer, metadata M.Metadata) error {
	packetHeader := buffer.To(aes.BlockSize)
	s.udpBlockCipher.Decrypt(packetHeader, packetHeader)

	sessionId := binary.BigEndian.Uint64(packetHeader)

	var _eiHeader [aes.BlockSize]byte
	eiHeader := _eiHeader[:]
	s.udpBlockCipher.Decrypt(eiHeader, buffer.Range(aes.BlockSize, 2*aes.BlockSize))
	xorWords(eiHeader, eiHeader, packetHeader)

	var user U
	if u, loaded := s.uPSKHash[_eiHeader]; loaded {
		user = u
	} else {
		return E.New("invalid request")
	}

	s.uCipher[user].Encrypt(packetHeader, packetHeader)
	copy(buffer.Range(aes.BlockSize, 2*aes.BlockSize), packetHeader)
	buffer.Advance(aes.BlockSize)

	metadata.Protocol = "shadowsocks-relay"
	metadata.Destination = s.uDestination[user]
	s.udpNat.NewContextPacket(ctx, sessionId, buffer, metadata, func(natConn N.PacketConn) (context.Context, N.PacketWriter) {
		return auth.ContextWithUser(ctx, user), &udpnat.DirectBackWriter{Source: conn, Nat: natConn}
	})
	return nil
}

func (s *RelayService[U]) NewError(ctx context.Context, err error) {
	s.handler.NewError(ctx, err)
}
