package shadowaead

import (
	"crypto/cipher"
	"encoding/binary"
	"io"
	"sync"

	"github.com/sagernet/sing/common/buf"
)

// https://shadowsocks.org/en/wiki/AEAD-Ciphers.html
const (
	MaxPacketSize          = 16*1024 - 1
	PacketLengthBufferSize = 2
)

const (
	// Overhead
	// crypto/cipher.gcmTagSize
	// golang.org/x/crypto/chacha20poly1305.Overhead
	Overhead = 16
)

type Reader struct {
	upstream io.Reader
	cipher   cipher.AEAD
	buffer   []byte
	nonce    []byte
	index    int
	cached   int
}

func NewReader(upstream io.Reader, cipher cipher.AEAD, maxPacketSize int) *Reader {
	return &Reader{
		upstream: upstream,
		cipher:   cipher,
		buffer:   make([]byte, maxPacketSize+Overhead),
		nonce:    make([]byte, cipher.NonceSize()),
	}
}

func NewRawReader(upstream io.Reader, cipher cipher.AEAD, buffer []byte, nonce []byte) *Reader {
	return &Reader{
		upstream: upstream,
		cipher:   cipher,
		buffer:   buffer,
		nonce:    nonce,
	}
}

func (r *Reader) Upstream() any {
	return r.upstream
}

func (r *Reader) WriteTo(writer io.Writer) (n int64, err error) {
	if r.cached > 0 {
		writeN, writeErr := writer.Write(r.buffer[r.index : r.index+r.cached])
		if writeErr != nil {
			return int64(writeN), writeErr
		}
		n += int64(writeN)
	}
	for {
		start := PacketLengthBufferSize + Overhead
		_, err = io.ReadFull(r.upstream, r.buffer[:start])
		if err != nil {
			return
		}
		_, err = r.cipher.Open(r.buffer[:0], r.nonce, r.buffer[:start], nil)
		if err != nil {
			return
		}
		increaseNonce(r.nonce)
		length := int(binary.BigEndian.Uint16(r.buffer[:PacketLengthBufferSize]))
		end := length + Overhead
		_, err = io.ReadFull(r.upstream, r.buffer[:end])
		if err != nil {
			return
		}
		_, err = r.cipher.Open(r.buffer[:0], r.nonce, r.buffer[:end], nil)
		if err != nil {
			return
		}
		increaseNonce(r.nonce)
		writeN, writeErr := writer.Write(r.buffer[:length])
		if writeErr != nil {
			return int64(writeN), writeErr
		}
		n += int64(writeN)
	}
}

func (r *Reader) readInternal() (err error) {
	start := PacketLengthBufferSize + Overhead
	_, err = io.ReadFull(r.upstream, r.buffer[:start])
	if err != nil {
		return err
	}
	_, err = r.cipher.Open(r.buffer[:0], r.nonce, r.buffer[:start], nil)
	if err != nil {
		return err
	}
	increaseNonce(r.nonce)
	length := int(binary.BigEndian.Uint16(r.buffer[:PacketLengthBufferSize]))
	end := length + Overhead
	_, err = io.ReadFull(r.upstream, r.buffer[:end])
	if err != nil {
		return err
	}
	_, err = r.cipher.Open(r.buffer[:0], r.nonce, r.buffer[:end], nil)
	if err != nil {
		return err
	}
	increaseNonce(r.nonce)
	r.cached = length
	r.index = 0
	return nil
}

func (r *Reader) ReadByte() (byte, error) {
	if r.cached == 0 {
		err := r.readInternal()
		if err != nil {
			return 0, err
		}
	}
	index := r.index
	r.index++
	r.cached--
	return r.buffer[index], nil
}

func (r *Reader) Read(b []byte) (n int, err error) {
	if r.cached > 0 {
		n = copy(b, r.buffer[r.index:r.index+r.cached])
		r.cached -= n
		r.index += n
		return
	}
	start := PacketLengthBufferSize + Overhead
	_, err = io.ReadFull(r.upstream, r.buffer[:start])
	if err != nil {
		return 0, err
	}
	_, err = r.cipher.Open(r.buffer[:0], r.nonce, r.buffer[:start], nil)
	if err != nil {
		return 0, err
	}
	increaseNonce(r.nonce)
	length := int(binary.BigEndian.Uint16(r.buffer[:PacketLengthBufferSize]))
	end := length + Overhead

	if len(b) >= end {
		data := b[:end]
		_, err = io.ReadFull(r.upstream, data)
		if err != nil {
			return 0, err
		}
		_, err = r.cipher.Open(b[:0], r.nonce, data, nil)
		if err != nil {
			return 0, err
		}
		increaseNonce(r.nonce)
		return length, nil
	} else {
		_, err = io.ReadFull(r.upstream, r.buffer[:end])
		if err != nil {
			return 0, err
		}
		_, err = r.cipher.Open(r.buffer[:0], r.nonce, r.buffer[:end], nil)
		if err != nil {
			return 0, err
		}
		increaseNonce(r.nonce)
		n = copy(b, r.buffer[:length])
		r.cached = length - n
		r.index = n
		return
	}
}

func (r *Reader) Discard(n int) error {
	for {
		if r.cached >= n {
			r.cached -= n
			r.index += n
			return nil
		} else if r.cached > 0 {
			n -= r.cached
			r.cached = 0
			r.index = 0
		}
		err := r.readInternal()
		if err != nil {
			return err
		}
	}
}

func (r *Reader) Buffer() *buf.Buffer {
	buffer := buf.With(r.buffer)
	buffer.Resize(r.index, r.cached)
	return buffer
}

func (r *Reader) Cached() int {
	return r.cached
}

func (r *Reader) CachedSlice() []byte {
	return r.buffer[r.index : r.index+r.cached]
}

func (r *Reader) ReadWithLengthChunk(lengthChunk []byte) error {
	_, err := r.cipher.Open(r.buffer[:0], r.nonce, lengthChunk, nil)
	if err != nil {
		return err
	}
	increaseNonce(r.nonce)
	length := int(binary.BigEndian.Uint16(r.buffer[:PacketLengthBufferSize]))
	end := length + Overhead
	_, err = io.ReadFull(r.upstream, r.buffer[:end])
	if err != nil {
		return err
	}
	_, err = r.cipher.Open(r.buffer[:0], r.nonce, r.buffer[:end], nil)
	if err != nil {
		return err
	}
	increaseNonce(r.nonce)
	r.cached = length
	r.index = 0
	return nil
}

func (r *Reader) ReadWithLength(length uint16) error {
	end := int(length) + Overhead
	_, err := io.ReadFull(r.upstream, r.buffer[:end])
	if err != nil {
		return err
	}
	_, err = r.cipher.Open(r.buffer[:0], r.nonce, r.buffer[:end], nil)
	if err != nil {
		return err
	}
	increaseNonce(r.nonce)
	r.cached = int(length)
	r.index = 0
	return nil
}

func (r *Reader) ReadExternalChunk(chunk []byte) error {
	bb, err := r.cipher.Open(r.buffer[:0], r.nonce, chunk, nil)
	if err != nil {
		return err
	}
	increaseNonce(r.nonce)
	r.cached = len(bb)
	r.index = 0
	return nil
}

func (r *Reader) ReadChunk(buffer *buf.Buffer, chunk []byte) error {
	bb, err := r.cipher.Open(buffer.Index(buffer.Len()), r.nonce, chunk, nil)
	if err != nil {
		return err
	}
	increaseNonce(r.nonce)
	buffer.Extend(len(bb))
	return nil
}

type Writer struct {
	upstream      io.Writer
	cipher        cipher.AEAD
	maxPacketSize int
	buffer        []byte
	nonce         []byte
	access        sync.Mutex
}

func NewWriter(upstream io.Writer, cipher cipher.AEAD, maxPacketSize int) *Writer {
	return &Writer{
		upstream:      upstream,
		cipher:        cipher,
		buffer:        make([]byte, maxPacketSize+PacketLengthBufferSize+Overhead*2),
		nonce:         make([]byte, cipher.NonceSize()),
		maxPacketSize: maxPacketSize,
	}
}

func NewRawWriter(upstream io.Writer, cipher cipher.AEAD, maxPacketSize int, buffer []byte, nonce []byte) *Writer {
	return &Writer{
		upstream:      upstream,
		cipher:        cipher,
		maxPacketSize: maxPacketSize,
		buffer:        buffer,
		nonce:         nonce,
	}
}

func (w *Writer) Upstream() any {
	return w.upstream
}

func (w *Writer) ReadFrom(r io.Reader) (n int64, err error) {
	for {
		offset := Overhead + PacketLengthBufferSize
		readN, readErr := r.Read(w.buffer[offset : offset+w.maxPacketSize])
		if readErr != nil {
			return 0, readErr
		}
		binary.BigEndian.PutUint16(w.buffer[:PacketLengthBufferSize], uint16(readN))
		w.cipher.Seal(w.buffer[:0], w.nonce, w.buffer[:PacketLengthBufferSize], nil)
		increaseNonce(w.nonce)
		packet := w.cipher.Seal(w.buffer[offset:offset], w.nonce, w.buffer[offset:offset+readN], nil)
		increaseNonce(w.nonce)
		_, err = w.upstream.Write(w.buffer[:offset+len(packet)])
		if err != nil {
			return
		}
		n += int64(readN)
	}
}

func (w *Writer) Write(p []byte) (n int, err error) {
	if len(p) == 0 {
		return
	}

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
		w.access.Lock()
		binary.BigEndian.PutUint16(w.buffer[:PacketLengthBufferSize], uint16(len(data)))
		w.cipher.Seal(w.buffer[:0], w.nonce, w.buffer[:PacketLengthBufferSize], nil)
		increaseNonce(w.nonce)
		offset := Overhead + PacketLengthBufferSize
		packet := w.cipher.Seal(w.buffer[offset:offset], w.nonce, data, nil)
		increaseNonce(w.nonce)
		w.access.Unlock()
		_, err = w.upstream.Write(w.buffer[:offset+len(packet)])
		if err != nil {
			return
		}
		n += len(data)
	}

	return
}

func (w *Writer) WriteVectorised(buffers []*buf.Buffer) error {
	defer buf.ReleaseMulti(buffers)
	var index int
	var err error
	for _, buffer := range buffers {
		pLen := buffer.Len()
		if pLen > w.maxPacketSize {
			_, err = w.Write(buffer.Bytes())
			if err != nil {
				return err
			}
		} else {
			if cap(w.buffer) < index+PacketLengthBufferSize+pLen+2*Overhead {
				_, err = w.upstream.Write(w.buffer[:index])
				index = 0
				if err != nil {
					return err
				}
			}
			w.access.Lock()
			binary.BigEndian.PutUint16(w.buffer[index:index+PacketLengthBufferSize], uint16(pLen))
			w.cipher.Seal(w.buffer[index:index], w.nonce, w.buffer[index:index+PacketLengthBufferSize], nil)
			increaseNonce(w.nonce)
			offset := index + Overhead + PacketLengthBufferSize
			w.cipher.Seal(w.buffer[offset:offset], w.nonce, buffer.Bytes(), nil)
			increaseNonce(w.nonce)
			w.access.Unlock()
			index = offset + pLen + Overhead
		}
	}
	if index > 0 {
		_, err = w.upstream.Write(w.buffer[:index])
	}
	return err
}

func (w *Writer) Buffer() *buf.Buffer {
	return buf.With(w.buffer)
}

func (w *Writer) WriteChunk(buffer *buf.Buffer, chunk []byte) {
	bb := w.cipher.Seal(buffer.Index(buffer.Len()), w.nonce, chunk, nil)
	buffer.Extend(len(bb))
	increaseNonce(w.nonce)
}

func (w *Writer) BufferedWriter(reversed int) *BufferedWriter {
	return &BufferedWriter{
		upstream: w,
		reversed: reversed,
		data:     w.buffer[PacketLengthBufferSize+Overhead : len(w.buffer)-Overhead],
	}
}

type BufferedWriter struct {
	upstream *Writer
	data     []byte
	reversed int
	index    int
}

func (w *BufferedWriter) Write(p []byte) (n int, err error) {
	for {
		cachedN := copy(w.data[w.reversed+w.index:], p[n:])
		w.index += cachedN
		if cachedN == len(p[n:]) {
			n += cachedN
			return
		}
		err = w.Flush()
		if err != nil {
			return
		}
		n += cachedN
	}
}

func (w *BufferedWriter) Flush() error {
	if w.index == 0 {
		if w.reversed > 0 {
			_, err := w.upstream.upstream.Write(w.upstream.buffer[:w.reversed])
			w.reversed = 0
			return err
		}
		return nil
	}
	buffer := w.upstream.buffer[w.reversed:]
	binary.BigEndian.PutUint16(buffer[:PacketLengthBufferSize], uint16(w.index))
	w.upstream.cipher.Seal(buffer[:0], w.upstream.nonce, buffer[:PacketLengthBufferSize], nil)
	increaseNonce(w.upstream.nonce)
	offset := Overhead + PacketLengthBufferSize
	packet := w.upstream.cipher.Seal(buffer[offset:offset], w.upstream.nonce, buffer[offset:offset+w.index], nil)
	increaseNonce(w.upstream.nonce)
	_, err := w.upstream.upstream.Write(w.upstream.buffer[:w.reversed+offset+len(packet)])
	w.reversed = 0
	w.index = 0
	return err
}

func increaseNonce(nonce []byte) {
	for i := range nonce {
		nonce[i]++
		if nonce[i] != 0 {
			return
		}
	}
}
