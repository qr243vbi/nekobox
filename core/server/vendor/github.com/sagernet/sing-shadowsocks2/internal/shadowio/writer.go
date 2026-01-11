package shadowio

import (
	"crypto/cipher"
	"encoding/binary"
	"io"
	"sync"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	N "github.com/sagernet/sing/common/network"
)

type Writer struct {
	WriterInterface
	writer        N.ExtendedWriter
	cipher        cipher.AEAD
	maxPacketSize int
	nonce         []byte
	access        sync.Mutex
}

func NewWriter(writer io.Writer, cipher cipher.AEAD, nonce []byte, maxPacketSize int) *Writer {
	if len(nonce) == 0 {
		nonce = make([]byte, cipher.NonceSize())
	}
	return &Writer{
		writer:        bufio.NewExtendedWriter(writer),
		cipher:        cipher,
		nonce:         nonce,
		maxPacketSize: maxPacketSize,
	}
}

func (w *Writer) Encrypt(destination []byte, source []byte) {
	w.cipher.Seal(destination, w.nonce, source, nil)
	increaseNonce(w.nonce)
}

func (w *Writer) Write(p []byte) (n int, err error) {
	if len(p) == 0 {
		return
	}
	w.access.Lock()
	defer w.access.Unlock()
	for pLen := len(p); pLen > 0; {
		var data []byte
		if pLen > w.maxPacketSize {
			data = p[:w.maxPacketSize]
			p = p[w.maxPacketSize:]
			pLen -= w.maxPacketSize
		} else {
			data = p
			pLen = 0
		}
		bufferSize := PacketLengthBufferSize + 2*Overhead + len(data)
		buffer := buf.NewSize(bufferSize)
		common.Must(binary.Write(buffer, binary.BigEndian, uint16(len(data))))
		w.cipher.Seal(buffer.Index(0), w.nonce, buffer.To(PacketLengthBufferSize), nil)
		increaseNonce(w.nonce)
		buffer.Extend(Overhead)
		w.cipher.Seal(buffer.Index(buffer.Len()), w.nonce, data, nil)
		buffer.Extend(len(data) + Overhead)
		increaseNonce(w.nonce)
		_, err = w.writer.Write(buffer.Bytes())
		buffer.Release()
		if err != nil {
			return
		}
		n += len(data)
	}
	return
}

func (w *Writer) WriteBuffer(buffer *buf.Buffer) error {
	if buffer.Len() > w.maxPacketSize {
		defer buffer.Release()
		return common.Error(w.Write(buffer.Bytes()))
	}
	pLen := buffer.Len()
	headerOffset := PacketLengthBufferSize + Overhead
	header := buffer.ExtendHeader(headerOffset)
	binary.BigEndian.PutUint16(header, uint16(pLen))
	w.cipher.Seal(header[:0], w.nonce, header[:PacketLengthBufferSize], nil)
	increaseNonce(w.nonce)
	w.cipher.Seal(buffer.Index(headerOffset), w.nonce, buffer.From(headerOffset), nil)
	increaseNonce(w.nonce)
	buffer.Extend(Overhead)
	return w.writer.WriteBuffer(buffer)
}

func (w *Writer) TakeNonce() []byte {
	return w.nonce
}

func (w *Writer) Upstream() any {
	return w.writer
}

type WriterInterface struct{}

func (w *WriterInterface) FrontHeadroom() int {
	return PacketLengthBufferSize + Overhead
}

func (w *WriterInterface) RearHeadroom() int {
	return Overhead
}
