package vmess

import (
	"encoding/binary"
	"hash/fnv"
	"io"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	"github.com/sagernet/sing/common/bufio"
	N "github.com/sagernet/sing/common/network"
)

type StreamChecksumReader struct {
	upstream N.ExtendedReader
}

func NewStreamChecksumReader(reader io.Reader) *StreamChecksumReader {
	return &StreamChecksumReader{bufio.NewExtendedReader(reader)}
}

func (r *StreamChecksumReader) Read(p []byte) (n int, err error) {
	n, err = r.upstream.Read(p)
	if err != nil {
		return
	}
	hash := fnv.New32a()
	common.Must1(hash.Write(p[4:n]))
	if hash.Sum32() != binary.BigEndian.Uint32(p) {
		return 0, ErrInvalidChecksum
	}
	n = copy(p, p[4:n])
	return
}

func (r *StreamChecksumReader) ReadBuffer(buffer *buf.Buffer) error {
	err := r.upstream.ReadBuffer(buffer)
	if err != nil {
		return err
	}
	hash := fnv.New32a()
	common.Must1(hash.Write(buffer.From(4)))
	if hash.Sum32() != binary.BigEndian.Uint32(buffer.To(4)) {
		return ErrInvalidChecksum
	}
	buffer.Advance(4)
	return nil
}

func (r *StreamChecksumReader) Upstream() any {
	return r.upstream
}

type StreamChecksumWriter struct {
	upstream *StreamChunkWriter
}

func NewStreamChecksumWriter(upstream *StreamChunkWriter) *StreamChecksumWriter {
	return &StreamChecksumWriter{upstream}
}

func (w *StreamChecksumWriter) Write(p []byte) (n int, err error) {
	hash := fnv.New32a()
	common.Must1(hash.Write(p))
	return w.upstream.WriteWithChecksum(hash.Sum32(), p)
}

func (w *StreamChecksumWriter) WriteBuffer(buffer *buf.Buffer) error {
	hash := fnv.New32a()
	common.Must1(hash.Write(buffer.Bytes()))
	hash.Sum(buffer.ExtendHeader(4)[:0])
	return common.Error(w.upstream.Write(buffer.Bytes()))
}

func (w *StreamChecksumWriter) FrontHeadroom() int {
	return 4
}

func (w *StreamChecksumWriter) Upstream() any {
	return w.upstream
}
