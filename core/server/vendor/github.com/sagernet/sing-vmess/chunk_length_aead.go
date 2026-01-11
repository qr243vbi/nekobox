package vmess

import (
	"crypto/cipher"
	"crypto/rand"
	"encoding/binary"
	"io"
	"sync"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	E "github.com/sagernet/sing/common/exceptions"
	N "github.com/sagernet/sing/common/network"

	"golang.org/x/crypto/sha3"
)

type AEADChunkReader struct {
	upstream      io.Reader
	cipher        cipher.AEAD
	globalPadding sha3.ShakeHash
	nonce         []byte
	nonceCount    uint16
}

func NewAEADChunkReader(upstream io.Reader, cipher cipher.AEAD, nonce []byte, globalPadding sha3.ShakeHash) *AEADChunkReader {
	readNonce := make([]byte, cipher.NonceSize())
	copy(readNonce, nonce)
	return &AEADChunkReader{
		upstream:      upstream,
		cipher:        cipher,
		nonce:         readNonce,
		globalPadding: globalPadding,
	}
}

func NewAes128GcmChunkReader(upstream io.Reader, key []byte, nonce []byte, globalPadding sha3.ShakeHash) *AEADChunkReader {
	return NewAEADChunkReader(upstream, newAesGcm(KDF(key, "auth_len")[:16]), nonce, globalPadding)
}

func NewChacha20Poly1305ChunkReader(upstream io.Reader, key []byte, nonce []byte, globalPadding sha3.ShakeHash) *AEADChunkReader {
	return NewAEADChunkReader(upstream, newChacha20Poly1305(GenerateChacha20Poly1305Key(KDF(key, "auth_len")[:16])), nonce, globalPadding)
}

func (r *AEADChunkReader) Read(p []byte) (n int, err error) {
	if cap(p) < 2+CipherOverhead {
		return 0, E.Extend(io.ErrShortBuffer, "AEAD chunk need ", 2+CipherOverhead)
	}
	_, err = io.ReadFull(r.upstream, p[:2+CipherOverhead])
	if err != nil {
		return
	}
	binary.BigEndian.PutUint16(r.nonce, r.nonceCount)
	r.nonceCount += 1
	_, err = r.cipher.Open(p[:0], r.nonce, p[:2+CipherOverhead], nil)
	if err != nil {
		return
	}
	length := binary.BigEndian.Uint16(p[:2])
	length += CipherOverhead
	dataLen := int(length)
	var paddingLen int
	if r.globalPadding != nil {
		var hashCode uint16
		common.Must(binary.Read(r.globalPadding, binary.BigEndian, &hashCode))
		paddingLen = int(hashCode % 64)
		dataLen -= paddingLen
	}
	if dataLen < 0 {
		err = E.Extend(ErrBadLengthChunk, "length=", length, ", padding=", paddingLen)
		return
	}
	if dataLen == 0 {
		err = io.EOF
		return
	}
	var readLen int
	readLen = len(p)
	if readLen > dataLen {
		readLen = dataLen
	} else if readLen < dataLen {
		return 0, E.Extend(io.ErrShortBuffer, "AEAD chunk need ", dataLen)
	}
	n, err = io.ReadFull(r.upstream, p[:readLen])
	if err != nil {
		return
	}
	_, err = io.CopyN(io.Discard, r.upstream, int64(paddingLen))
	return
}

func (r *AEADChunkReader) Upstream() any {
	return r.upstream
}

type AEADChunkWriter struct {
	upstream      N.ExtendedWriter
	cipher        cipher.AEAD
	globalPadding sha3.ShakeHash
	nonce         []byte
	nonceCount    uint16
	hashAccess    sync.Mutex
	writeAccess   sync.Mutex
}

func NewAEADChunkWriter(upstream io.Writer, cipher cipher.AEAD, nonce []byte, globalPadding sha3.ShakeHash) *AEADChunkWriter {
	writeNonce := make([]byte, cipher.NonceSize())
	copy(writeNonce, nonce)
	return &AEADChunkWriter{
		upstream:      bufio.NewExtendedWriter(upstream),
		cipher:        cipher,
		nonce:         writeNonce,
		globalPadding: globalPadding,
	}
}

func NewAes128GcmChunkWriter(upstream io.Writer, key []byte, nonce []byte, globalPadding sha3.ShakeHash) *AEADChunkWriter {
	return NewAEADChunkWriter(upstream, newAesGcm(KDF(key, "auth_len")[:16]), nonce, globalPadding)
}

func NewChacha20Poly1305ChunkWriter(upstream io.Writer, key []byte, nonce []byte, globalPadding sha3.ShakeHash) *AEADChunkWriter {
	return NewAEADChunkWriter(upstream, newChacha20Poly1305(GenerateChacha20Poly1305Key(KDF(key, "auth_len")[:16])), nonce, globalPadding)
}

func (w *AEADChunkWriter) Write(p []byte) (n int, err error) {
	dataLength := uint16(len(p))
	var paddingLen uint16
	if w.globalPadding != nil {
		w.hashAccess.Lock()
		var hashCode uint16
		common.Must(binary.Read(w.globalPadding, binary.BigEndian, &hashCode))
		paddingLen = hashCode % MaxPaddingSize
		dataLength += paddingLen
		w.hashAccess.Unlock()
	}
	dataLength -= CipherOverhead

	lengthBuffer := buf.NewSize(2 + CipherOverhead)
	binary.BigEndian.PutUint16(lengthBuffer.Extend(2), dataLength)

	binary.BigEndian.PutUint16(w.nonce, w.nonceCount)
	w.nonceCount += 1
	w.cipher.Seal(lengthBuffer.Index(0), w.nonce, lengthBuffer.Bytes(), nil)
	lengthBuffer.Extend(CipherOverhead)

	w.writeAccess.Lock()
	_, err = lengthBuffer.WriteTo(w.upstream)
	if err != nil {
		return
	}

	lengthBuffer.Release()

	n, err = w.upstream.Write(p)
	if err != nil {
		return
	}
	if paddingLen > 0 {
		_, err = io.CopyN(w.upstream, rand.Reader, int64(paddingLen))
		if err != nil {
			return
		}
	}
	w.writeAccess.Unlock()
	return
}

func (w *AEADChunkWriter) WriteBuffer(buffer *buf.Buffer) error {
	dataLength := uint16(buffer.Len())
	var paddingLen uint16
	if w.globalPadding != nil {
		w.hashAccess.Lock()
		var hashCode uint16
		common.Must(binary.Read(w.globalPadding, binary.BigEndian, &hashCode))
		paddingLen = hashCode % MaxPaddingSize
		dataLength += paddingLen
		w.hashAccess.Unlock()
	}
	dataLength -= CipherOverhead
	lengthBuffer := buffer.ExtendHeader(2 + CipherOverhead)
	binary.BigEndian.PutUint16(lengthBuffer, dataLength)
	binary.BigEndian.PutUint16(w.nonce, w.nonceCount)
	w.nonceCount += 1
	w.cipher.Seal(lengthBuffer[:0], w.nonce, lengthBuffer[:2], nil)
	if paddingLen > 0 {
		_, err := buffer.ReadFullFrom(rand.Reader, int(paddingLen))
		if err != nil {
			buffer.Release()
			return err
		}
	}
	return w.upstream.WriteBuffer(buffer)
}

func (w *AEADChunkWriter) FrontHeadroom() int {
	return 2 + CipherOverhead
}

func (w *AEADChunkWriter) RearHeadroom() int {
	if w.globalPadding != nil {
		return CipherOverhead + MaxPaddingSize
	} else {
		return CipherOverhead
	}
}

func (w *AEADChunkWriter) Upstream() any {
	return w.upstream
}
