package network

import (
	"io"
	"syscall"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
)

type ReadWaitable interface {
	InitializeReadWaiter(options ReadWaitOptions) (needCopy bool)
}

type ReadWaitOptions struct {
	FrontHeadroom  int
	RearHeadroom   int
	MTU            int
	IncreaseBuffer bool
	BatchSize      int
}

func NewReadWaitOptions(source any, destination any) ReadWaitOptions {
	return ReadWaitOptions{
		FrontHeadroom: CalculateFrontHeadroom(destination),
		RearHeadroom:  CalculateRearHeadroom(destination),
		MTU:           CalculateMTU(source, destination),
	}
}

func (o ReadWaitOptions) NeedHeadroom() bool {
	return o.FrontHeadroom > 0 || o.RearHeadroom > 0
}

func (o ReadWaitOptions) Copy(buffer *buf.Buffer) *buf.Buffer {
	if o.FrontHeadroom > buffer.Start() ||
		o.RearHeadroom > buffer.FreeLen() {
		newBuffer := o.newBuffer(buf.UDPBufferSize, false)
		newBuffer.Write(buffer.Bytes())
		buffer.Release()
		return newBuffer
	} else {
		return buffer
	}
}

func (o ReadWaitOptions) NewBuffer() *buf.Buffer {
	return o.newBuffer(buf.BufferSize, true)
}

func (o ReadWaitOptions) NewPacketBuffer() *buf.Buffer {
	return o.newBuffer(buf.UDPBufferSize, true)
}

func (o ReadWaitOptions) newBuffer(defaultBufferSize int, reserve bool) *buf.Buffer {
	var bufferSize int
	if o.IncreaseBuffer {
		bufferSize = 65535
	} else if o.MTU > 0 {
		bufferSize = o.MTU + o.FrontHeadroom + o.RearHeadroom
	} else {
		bufferSize = defaultBufferSize
	}
	buffer := buf.NewSize(bufferSize)
	if o.FrontHeadroom > 0 {
		buffer.Resize(o.FrontHeadroom, 0)
	}
	if o.RearHeadroom > 0 && reserve {
		buffer.Reserve(o.RearHeadroom)
	}
	return buffer
}

func (o ReadWaitOptions) PostReturn(buffer *buf.Buffer) {
	if o.RearHeadroom > 0 {
		buffer.OverCap(o.RearHeadroom)
	}
}

type ReadWaiter interface {
	ReadWaitable
	WaitReadBuffer() (buffer *buf.Buffer, err error)
}

type ReadWaitCreator interface {
	CreateReadWaiter() (ReadWaiter, bool)
}

type VectorisedReadWaiter interface {
	ReadWaitable
	WaitReadBuffers() (buffers []*buf.Buffer, err error)
}

type VectorisedReadWaitCreator interface {
	CreateVectorisedReadWaiter() (VectorisedReadWaiter, bool)
}

type PacketReadWaiter interface {
	ReadWaitable
	WaitReadPacket() (buffer *buf.Buffer, destination M.Socksaddr, err error)
}

type PacketReadWaitCreator interface {
	CreateReadWaiter() (PacketReadWaiter, bool)
}

type VectorisedPacketReadWaiter interface {
	ReadWaitable
	WaitReadPackets() (buffers []*buf.Buffer, destinations []M.Socksaddr, err error)
}

type VectorisedPacketReadWaitCreator interface {
	CreateVectorisedPacketReadWaiter() (VectorisedPacketReadWaiter, bool)
}

type SyscallReader interface {
	SyscallConnForRead() syscall.RawConn
	HandleSyscallReadError(inputErr error) ([]byte, error)
}

func SyscallAvailableForRead(reader io.Reader) bool {
	if _, ok := reader.(syscall.Conn); ok {
		return true
	}
	if _, ok := reader.(SyscallReader); ok {
		return true
	}
	if u, ok := reader.(ReaderWithUpstream); !ok || !u.ReaderReplaceable() {
		return false
	}
	if u, ok := reader.(WithUpstreamReader); ok {
		return SyscallAvailableForRead(u.UpstreamReader().(io.Reader))
	}
	if u, ok := reader.(common.WithUpstream); ok {
		return SyscallAvailableForRead(u.Upstream().(io.Reader))
	}
	return false
}

func SyscallConnForRead(reader io.Reader) (SyscallReader, syscall.RawConn) {
	if c, ok := reader.(syscall.Conn); ok {
		conn, _ := c.SyscallConn()
		return nil, conn
	}
	if c, ok := reader.(SyscallReader); ok {
		return c, c.SyscallConnForRead()
	}
	if u, ok := reader.(ReaderWithUpstream); !ok || !u.ReaderReplaceable() {
		return nil, nil
	}
	if u, ok := reader.(WithUpstreamReader); ok {
		return SyscallConnForRead(u.UpstreamReader().(io.Reader))
	}
	if u, ok := reader.(common.WithUpstream); ok {
		return SyscallConnForRead(u.Upstream().(io.Reader))
	}
	return nil, nil
}

type SyscallWriter interface {
	SyscallConnForWrite() syscall.RawConn
}

func SyscallAvailableForWrite(writer io.Writer) bool {
	if _, ok := writer.(syscall.Conn); ok {
		return true
	}
	if _, ok := writer.(SyscallWriter); ok {
		return true
	}
	if u, ok := writer.(WriterWithUpstream); !ok || !u.WriterReplaceable() {
		return false
	}
	if u, ok := writer.(WithUpstreamWriter); ok {
		return SyscallAvailableForWrite(u.UpstreamWriter().(io.Writer))
	}
	if u, ok := writer.(common.WithUpstream); ok {
		return SyscallAvailableForWrite(u.Upstream().(io.Writer))
	}
	return false
}

func SyscallConnForWrite(writer io.Writer) (SyscallWriter, syscall.RawConn) {
	if c, ok := writer.(syscall.Conn); ok {
		conn, _ := c.SyscallConn()
		return nil, conn
	}
	if c, ok := writer.(SyscallWriter); ok {
		return c, c.SyscallConnForWrite()
	}
	if u, ok := writer.(WriterWithUpstream); !ok || !u.WriterReplaceable() {
		return nil, nil
	}
	if u, ok := writer.(WithUpstreamWriter); ok {
		return SyscallConnForWrite(u.UpstreamWriter().(io.Writer))
	}
	if u, ok := writer.(common.WithUpstream); ok {
		return SyscallConnForWrite(u.Upstream().(io.Writer))
	}
	return nil, nil
}
