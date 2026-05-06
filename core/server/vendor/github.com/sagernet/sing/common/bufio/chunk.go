package bufio

import (
	"io"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	N "github.com/sagernet/sing/common/network"
)

type ChunkReader struct {
	upstream     N.ExtendedReader
	maxChunkSize int
	cache        *buf.Buffer
}

func NewChunkReader(upstream io.Reader, maxChunkSize int) *ChunkReader {
	return &ChunkReader{
		upstream:     NewExtendedReader(upstream),
		maxChunkSize: maxChunkSize,
	}
}

func (c *ChunkReader) ReadBuffer(buffer *buf.Buffer) error {
	if buffer.FreeLen() >= c.maxChunkSize {
		return c.upstream.ReadBuffer(buffer)
	}
	if c.cache == nil {
		c.cache = buf.NewSize(c.maxChunkSize)
	} else if !c.cache.IsEmpty() {
		return common.Error(buffer.ReadFrom(c.cache))
	}
	c.cache.Reset()
	err := c.upstream.ReadBuffer(c.cache)
	if err != nil {
		c.cache.Release()
		c.cache = nil
		return err
	}
	return common.Error(buffer.ReadFrom(c.cache))
}

func (c *ChunkReader) Read(p []byte) (n int, err error) {
	if c.cache == nil {
		c.cache = buf.NewSize(c.maxChunkSize)
	} else if !c.cache.IsEmpty() {
		return c.cache.Read(p)
	}
	c.cache.Reset()
	err = c.upstream.ReadBuffer(c.cache)
	if err != nil {
		c.cache.Release()
		c.cache = nil
		return
	}
	return c.cache.Read(p)
}

func (c *ChunkReader) ReadByte() (byte, error) {
	buffer, err := c.ReadChunk()
	if err != nil {
		return 0, err
	}
	return buffer.ReadByte()
}

func (c *ChunkReader) ReadChunk() (*buf.Buffer, error) {
	if c.cache == nil {
		c.cache = buf.NewSize(c.maxChunkSize)
	} else if !c.cache.IsEmpty() {
		return c.cache, nil
	}
	c.cache.Reset()
	err := c.upstream.ReadBuffer(c.cache)
	if err != nil {
		c.cache.Release()
		c.cache = nil
		return nil, err
	}
	return c.cache, nil
}

func (c *ChunkReader) MTU() int {
	return c.maxChunkSize
}

type ChunkWriter struct {
	upstream     N.ExtendedWriter
	maxChunkSize int
}

func NewChunkWriter(writer io.Writer, maxChunkSize int) *ChunkWriter {
	return &ChunkWriter{
		upstream:     NewExtendedWriter(writer),
		maxChunkSize: maxChunkSize,
	}
}

func (w *ChunkWriter) Write(p []byte) (n int, err error) {
	for pLen := len(p); pLen > 0; {
		var data []byte
		if pLen > w.maxChunkSize {
			data = p[:w.maxChunkSize]
			p = p[w.maxChunkSize:]
			pLen -= w.maxChunkSize
		} else {
			data = p
			pLen = 0
		}
		var writeN int
		writeN, err = w.upstream.Write(data)
		n += writeN
		if err != nil {
			return
		}
	}
	return
}

func (w *ChunkWriter) WriteBuffer(buffer *buf.Buffer) error {
	if buffer.Len() > w.maxChunkSize {
		defer buffer.Release()
		return common.Error(w.Write(buffer.Bytes()))
	}
	return w.upstream.WriteBuffer(buffer)
}

func (w *ChunkWriter) Upstream() any {
	return w.upstream
}

func (w *ChunkWriter) MTU() int {
	return w.maxChunkSize
}
