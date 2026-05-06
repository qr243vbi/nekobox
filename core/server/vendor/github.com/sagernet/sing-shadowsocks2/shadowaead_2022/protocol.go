package shadowaead_2022

import (
	"crypto/cipher"
	"crypto/sha256"
	"io"
	"sync/atomic"

	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/random"

	"lukechampine.com/blake3"
)

const (
	HeaderTypeClient              = 0
	HeaderTypeServer              = 1
	MaxPaddingLength              = 900
	PacketNonceSize               = 24
	RequestHeaderFixedChunkLength = 1 + 8 + 2
	PacketMinimalHeaderSize       = 30
)

var (
	ErrNoEIH                 = E.New("Shadowsocks 2022 EIH support only available in AES ciphers")
	ErrBadHeaderType         = E.New("bad header type")
	ErrBadTimestamp          = E.New("bad timestamp")
	ErrBadRequestSalt        = E.New("bad request salt")
	ErrSaltNotUnique         = E.New("salt not unique")
	ErrBadClientSessionId    = E.New("bad client session id")
	ErrPacketIdNotUnique     = E.New("packet id not unique")
	ErrTooManyServerSessions = E.New("server session changed more than once during the last minute")
)

func init() {
	random.InitializeSeed()
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

func Blake3KeyedHash(reader io.Reader) io.Reader {
	key := make([]byte, 32)
	common.Must1(io.ReadFull(reader, key))
	h := blake3.New(1024, key)
	return h.XOF()
}
