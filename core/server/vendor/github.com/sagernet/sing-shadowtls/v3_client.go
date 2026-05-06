package shadowtls

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha1"
	"crypto/sha256"
	"encoding/binary"
	"hash"
	"io"
	"net"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
)

func generateSessionID(password string) func(clientHello []byte, sessionID []byte) error {
	return func(clientHello []byte, sessionID []byte) error {
		const sessionIDStart = 1 + 3 + 2 + tlsRandomSize + 1
		if len(clientHello) < sessionIDStart+tlsSessionIDSize {
			return E.New("unexpected client hello length")
		}
		_, err := rand.Read(sessionID[:tlsSessionIDSize-hmacSize])
		if err != nil {
			return err
		}
		hmacSHA1Hash := hmac.New(sha1.New, []byte(password))
		hmacSHA1Hash.Write(clientHello[:sessionIDStart])
		hmacSHA1Hash.Write(sessionID)
		hmacSHA1Hash.Write(clientHello[sessionIDStart+tlsSessionIDSize:])
		copy(sessionID[tlsSessionIDSize-hmacSize:], hmacSHA1Hash.Sum(nil)[:hmacSize])
		return nil
	}
}

type streamWrapper struct {
	net.Conn
	password     string
	buffer       *buf.Buffer
	serverRandom []byte
	readHMAC     hash.Hash
	readHMACKey  []byte
	isTLS13      bool
	authorized   bool
}

func newStreamWrapper(conn net.Conn, password string) *streamWrapper {
	return &streamWrapper{
		Conn:     conn,
		password: password,
	}
}

func (w *streamWrapper) Authorized() (bool, bool, []byte, hash.Hash) {
	return w.isTLS13, w.authorized, w.serverRandom, w.readHMAC
}

func (w *streamWrapper) Read(p []byte) (n int, err error) {
	if w.buffer != nil {
		if !w.buffer.IsEmpty() {
			return w.buffer.Read(p)
		}
		w.buffer.Release()
		w.buffer = nil
	}
	var tlsHeader [tlsHeaderSize]byte
	_, err = io.ReadFull(w.Conn, tlsHeader[:])
	if err != nil {
		return
	}
	length := int(binary.BigEndian.Uint16(tlsHeader[3:tlsHeaderSize]))
	w.buffer = buf.NewSize(tlsHeaderSize + length)
	common.Must1(w.buffer.Write(tlsHeader[:]))
	_, err = w.buffer.ReadFullFrom(w.Conn, length)
	if err != nil {
		return
	}
	buffer := w.buffer.Bytes()
	switch tlsHeader[0] {
	case handshake:
		if len(buffer) > serverRandomIndex+tlsRandomSize && buffer[5] == serverHello {
			w.serverRandom = make([]byte, tlsRandomSize)
			copy(w.serverRandom, buffer[serverRandomIndex:serverRandomIndex+tlsRandomSize])
			w.readHMAC = hmac.New(sha1.New, []byte(w.password))
			w.readHMAC.Write(w.serverRandom)
			w.readHMACKey = kdf(w.password, w.serverRandom)
			w.isTLS13 = isServerHelloSupportTLS13(buffer[5:])
			if !w.isTLS13 {
				w.authorized = true
			}
		}
	case applicationData:
		w.authorized = false
		if len(buffer) > tlsHmacHeaderSize && w.readHMAC != nil {
			w.readHMAC.Write(buffer[tlsHmacHeaderSize:])
			if hmac.Equal(w.readHMAC.Sum(nil)[:hmacSize], buffer[tlsHeaderSize:tlsHmacHeaderSize]) {
				xorSlice(buffer[tlsHmacHeaderSize:], w.readHMACKey)
				copy(buffer[hmacSize:], buffer[:tlsHeaderSize])
				binary.BigEndian.PutUint16(buffer[hmacSize+3:], uint16(len(buffer)-tlsHmacHeaderSize))
				w.buffer.Advance(hmacSize)
				w.authorized = true
			}
		}
	}
	return w.buffer.Read(p)
}

func kdf(password string, serverRandom []byte) []byte {
	hasher := sha256.New()
	hasher.Write([]byte(password))
	hasher.Write(serverRandom)
	return hasher.Sum(nil)
}

func xorSlice(data []byte, key []byte) {
	for i := range data {
		data[i] ^= key[i%len(key)]
	}
}
