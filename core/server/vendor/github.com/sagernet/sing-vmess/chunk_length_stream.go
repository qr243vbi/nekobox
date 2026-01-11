package vmess

import (
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

var ErrBadLengthChunk = E.New("bad length chunk")

type StreamChunkReader struct {
	upstream      io.Reader
	chunkMasking  sha3.ShakeHash
	globalPadding sha3.ShakeHash
}

func NewStreamChunkReader(upstream io.Reader, chunkMasking sha3.ShakeHash, globalPadding sha3.ShakeHash) *StreamChunkReader {
	return &StreamChunkReader{
		upstream:      upstream,
		chunkMasking:  chunkMasking,
		globalPadding: globalPadding,
	}
}

func (r *StreamChunkReader) Read(p []byte) (n int, err error) {
	var length uint16
	err = binary.Read(r.upstream, binary.BigEndian, &length)
	if err != nil {
		return
	}
	var paddingLen int
	if r.globalPadding != nil {
		var hashCode uint16
		common.Must(binary.Read(r.globalPadding, binary.BigEndian, &hashCode))
		paddingLen = int(hashCode % 64)
	}
	if r.chunkMasking != nil {
		var hashCode uint16
		common.Must(binary.Read(r.chunkMasking, binary.BigEndian, &hashCode))
		length ^= hashCode
	}
	dataLen := int(length)
	if paddingLen > 0 {
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
		return 0, E.Extend(io.ErrShortBuffer, "stream chunk need ", dataLen)
	}
	n, err = io.ReadFull(r.upstream, p[:readLen])
	if err != nil {
		return
	}
	_, err = io.CopyN(io.Discard, r.upstream, int64(paddingLen))
	return
}

func (r *StreamChunkReader) Upstream() any {
	return r.upstream
}

type StreamChunkWriter struct {
	upstream      N.ExtendedWriter
	chunkMasking  sha3.ShakeHash
	globalPadding sha3.ShakeHash
	hashAccess    sync.Mutex
	writeAccess   sync.Mutex
}

func NewStreamChunkWriter(upstream io.Writer, chunkMasking sha3.ShakeHash, globalPadding sha3.ShakeHash) *StreamChunkWriter {
	return &StreamChunkWriter{
		upstream:      bufio.NewExtendedWriter(upstream),
		chunkMasking:  chunkMasking,
		globalPadding: globalPadding,
	}
}

func (w *StreamChunkWriter) Write(p []byte) (n int, err error) {
	dataLen := uint16(len(p))
	var paddingLen uint16
	if w.globalPadding != nil || w.chunkMasking != nil {
		w.hashAccess.Lock()
		if w.globalPadding != nil {
			var hashCode uint16
			common.Must(binary.Read(w.globalPadding, binary.BigEndian, &hashCode))
			paddingLen = hashCode % MaxPaddingSize
			dataLen += paddingLen
		}
		if w.chunkMasking != nil {
			var hashCode uint16
			common.Must(binary.Read(w.chunkMasking, binary.BigEndian, &hashCode))
			dataLen ^= hashCode
		}
		w.hashAccess.Unlock()
	}
	w.writeAccess.Lock()
	err = binary.Write(w.upstream, binary.BigEndian, dataLen)
	if err != nil {
		return
	}
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

func (w *StreamChunkWriter) WriteBuffer(buffer *buf.Buffer) error {
	dataLen := uint16(buffer.Len())
	var paddingLen uint16
	if w.globalPadding != nil || w.chunkMasking != nil {
		w.hashAccess.Lock()
		if w.globalPadding != nil {
			var hashCode uint16
			common.Must(binary.Read(w.globalPadding, binary.BigEndian, &hashCode))
			paddingLen = hashCode % MaxPaddingSize
			dataLen += paddingLen
		}
		if w.chunkMasking != nil {
			var hashCode uint16
			common.Must(binary.Read(w.chunkMasking, binary.BigEndian, &hashCode))
			dataLen ^= hashCode
		}
		w.hashAccess.Unlock()
	}
	binary.BigEndian.PutUint16(buffer.ExtendHeader(2), dataLen)
	if paddingLen > 0 {
		_, err := buffer.ReadFullFrom(rand.Reader, int(paddingLen))
		if err != nil {
			buffer.Release()
			return err
		}
	}
	return w.upstream.WriteBuffer(buffer)
}

func (w *StreamChunkWriter) WriteWithChecksum(checksum uint32, p []byte) (n int, err error) {
	dataLen := uint16(4 + len(p))
	var paddingLen uint16
	if w.globalPadding != nil || w.chunkMasking != nil {
		w.hashAccess.Lock()
		if w.globalPadding != nil {
			var hashCode uint16
			common.Must(binary.Read(w.globalPadding, binary.BigEndian, &hashCode))
			paddingLen = hashCode % MaxPaddingSize
			dataLen += paddingLen
		}
		if w.chunkMasking != nil {
			var hashCode uint16
			common.Must(binary.Read(w.chunkMasking, binary.BigEndian, &hashCode))
			dataLen ^= hashCode
		}
		w.hashAccess.Unlock()
	}
	w.writeAccess.Lock()
	err = binary.Write(w.upstream, binary.BigEndian, dataLen)
	if err != nil {
		return
	}
	err = binary.Write(w.upstream, binary.BigEndian, checksum)
	if err != nil {
		return
	}
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

func (w *StreamChunkWriter) FrontHeadroom() int {
	return 2
}

func (w *StreamChunkWriter) RearHeadroom() int {
	if w.globalPadding != nil {
		return MaxPaddingSize
	} else {
		return 0
	}
}

func (w *StreamChunkWriter) Upstream() any {
	return w.upstream
}
