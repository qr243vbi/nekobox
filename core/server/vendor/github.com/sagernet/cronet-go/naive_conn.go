package cronet

import (
	"context"
	"encoding/binary"
	"io"
	"math/rand"
	"net"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/baderror"
	"github.com/sagernet/sing/common/buf"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
	"github.com/sagernet/sing/common/rw"
)

const (
	paddingCount        = 8
	maxPaddingChunkSize = 65535
)

func generatePaddingHeader() string {
	paddingLen := rand.Intn(32) + 30
	padding := make([]byte, paddingLen)
	bits := rand.Uint64()
	for i := 0; i < 16; i++ {
		padding[i] = "!#$()+<>?@[]^`{}"[bits&15]
		bits >>= 4
	}
	for i := 16; i < paddingLen; i++ {
		padding[i] = '~'
	}
	return string(padding)
}

type paddingConn struct {
	readPadding      int
	writePadding     int
	readRemaining    int
	paddingRemaining int
}

func (p *paddingConn) readWithPadding(reader io.Reader, buffer []byte) (n int, err error) {
	if p.readRemaining > 0 {
		if len(buffer) > p.readRemaining {
			buffer = buffer[:p.readRemaining]
		}
		n, err = reader.Read(buffer)
		if err != nil {
			return
		}
		p.readRemaining -= n
		return
	}
	if p.paddingRemaining > 0 {
		err = rw.SkipN(reader, p.paddingRemaining)
		if err != nil {
			return
		}
		p.paddingRemaining = 0
	}
	if p.readPadding < paddingCount {
		var paddingHeader []byte
		if len(buffer) >= 3 {
			paddingHeader = buffer[:3]
		} else {
			paddingHeader = make([]byte, 3)
		}
		_, err = io.ReadFull(reader, paddingHeader)
		if err != nil {
			return
		}
		originalDataSize := int(binary.BigEndian.Uint16(paddingHeader[:2]))
		paddingSize := int(paddingHeader[2])
		if len(buffer) > originalDataSize {
			buffer = buffer[:originalDataSize]
		}
		n, err = reader.Read(buffer)
		if err != nil {
			return
		}
		p.readPadding++
		p.readRemaining = originalDataSize - n
		p.paddingRemaining = paddingSize
		return
	}
	return reader.Read(buffer)
}

func (p *paddingConn) writeWithPadding(writer io.Writer, data []byte) (n int, err error) {
	if p.writePadding < paddingCount {
		paddingSize := rand.Intn(256)
		buffer := buf.NewSize(3 + len(data) + paddingSize)
		defer buffer.Release()
		header := buffer.Extend(3)
		binary.BigEndian.PutUint16(header, uint16(len(data)))
		header[2] = byte(paddingSize)
		common.Must1(buffer.Write(data))
		if paddingSize > 0 {
			common.Must(buffer.WriteZeroN(paddingSize))
		}
		_, err = writer.Write(buffer.Bytes())
		if err == nil {
			n = len(data)
		}
		p.writePadding++
		return
	}
	return writer.Write(data)
}

func (p *paddingConn) writeBufferWithPadding(writer io.Writer, buffer *buf.Buffer) error {
	if p.writePadding < paddingCount {
		bufferLen := buffer.Len()
		if bufferLen > maxPaddingChunkSize {
			_, err := p.writeChunked(writer, buffer.Bytes())
			return err
		}
		paddingSize := rand.Intn(256)
		header := buffer.ExtendHeader(3)
		binary.BigEndian.PutUint16(header, uint16(bufferLen))
		header[2] = byte(paddingSize)
		if paddingSize > 0 {
			common.Must(buffer.WriteZeroN(paddingSize))
		}
		p.writePadding++
	}
	return common.Error(writer.Write(buffer.Bytes()))
}

func (p *paddingConn) writeChunked(writer io.Writer, data []byte) (n int, err error) {
	for len(data) > 0 {
		var chunk []byte
		if len(data) > maxPaddingChunkSize {
			chunk = data[:maxPaddingChunkSize]
			data = data[maxPaddingChunkSize:]
		} else {
			chunk = data
			data = nil
		}
		var written int
		written, err = p.writeWithPadding(writer, chunk)
		n += written
		if err != nil {
			return
		}
	}
	return
}

func (p *paddingConn) frontHeadroom() int {
	if p.writePadding < paddingCount {
		return 3
	}
	return 0
}

func (p *paddingConn) rearHeadroom() int {
	if p.writePadding < paddingCount {
		return 255
	}
	return 0
}

func (p *paddingConn) writerMTU() int {
	if p.writePadding < paddingCount {
		return maxPaddingChunkSize
	}
	return 0
}

func (p *paddingConn) readerReplaceable() bool {
	return p.readPadding == paddingCount
}

func (p *paddingConn) writerReplaceable() bool {
	return p.writePadding == paddingCount
}

type NaiveConn interface {
	net.Conn
	Handshake() error
	HandshakeContext(ctx context.Context) error
}
type naiveConn struct {
	net.Conn
	ctx    context.Context
	conn   *BidirectionalConn
	logger logger.ContextLogger
	paddingConn
}

func NewNaiveConn(ctx context.Context, conn *BidirectionalConn, l logger.ContextLogger) NaiveConn {
	return &naiveConn{Conn: conn, ctx: ctx, conn: conn, logger: l}
}

func (c *naiveConn) Handshake() error {
	headers, err := c.conn.WaitForHeaders()
	if err != nil {
		c.logger.WarnContext(c.ctx, "handshake failed: ", err)
		return err
	}
	if headers[":status"] != "200" {
		err = E.New("unexpected response status: ", headers[":status"])
		c.logger.WarnContext(c.ctx, "handshake failed: ", err)
		return err
	}
	c.logger.DebugContext(c.ctx, "handshake succeeded")
	return nil
}

func (c *naiveConn) HandshakeContext(ctx context.Context) error {
	headers, err := c.conn.WaitForHeadersContext(ctx)
	if err != nil {
		c.logger.WarnContext(c.ctx, "handshake failed: ", err)
		return err
	}
	if headers[":status"] != "200" {
		err = E.New("unexpected response status: ", headers[":status"])
		c.logger.WarnContext(c.ctx, "handshake failed: ", err)
		return err
	}
	c.logger.DebugContext(c.ctx, "handshake succeeded")
	return nil
}

func (c *naiveConn) Read(p []byte) (n int, err error) {
	n, err = c.readWithPadding(c.Conn, p)
	return n, baderror.WrapH2(err)
}

func (c *naiveConn) Write(p []byte) (n int, err error) {
	n, err = c.writeChunked(c.Conn, p)
	return n, baderror.WrapH2(err)
}

func (c *naiveConn) WriteBuffer(buffer *buf.Buffer) error {
	defer buffer.Release()
	err := c.writeBufferWithPadding(c.Conn, buffer)
	return baderror.WrapH2(err)
}

func (c *naiveConn) FrontHeadroom() int      { return c.frontHeadroom() }
func (c *naiveConn) RearHeadroom() int       { return c.rearHeadroom() }
func (c *naiveConn) WriterMTU() int          { return c.writerMTU() }
func (c *naiveConn) Upstream() any           { return c.Conn }
func (c *naiveConn) ReaderReplaceable() bool { return c.readerReplaceable() }
func (c *naiveConn) WriterReplaceable() bool { return c.writerReplaceable() }
