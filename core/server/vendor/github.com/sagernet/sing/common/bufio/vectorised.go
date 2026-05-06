package bufio

import (
	"io"
	"net"
	"os"
	"runtime"
	"syscall"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

func NewVectorisedWriter(writer io.Writer) N.VectorisedWriter {
	if vectorisedWriter, ok := CreateVectorisedWriter(writer); ok {
		return vectorisedWriter
	}
	return &BufferedVectorisedWriter{upstream: writer}
}

func CreateVectorisedWriter(writer any) (N.VectorisedWriter, bool) {
	if runtime.GOOS == "windows" {
		switch conn := writer.(type) {
		case N.VectorisedWriter:
			return conn, true
		case *net.TCPConn:
			return &NetVectorisedWriterWrapper{conn}, true
		case *net.UDPConn:
			return &NetVectorisedWriterWrapper{conn}, true
		case *net.IPConn:
			return &NetVectorisedWriterWrapper{conn}, true
		case *net.UnixConn:
			return &NetVectorisedWriterWrapper{conn}, true
		}
	} else {
		switch conn := writer.(type) {
		case N.VectorisedWriter:
			return conn, true
		case syscall.Conn:
			rawConn, err := conn.SyscallConn()
			if err != nil {
				return nil, false
			}
			return &SyscallVectorisedWriter{upstream: writer, rawConn: rawConn}, true
		case syscall.RawConn:
			return &SyscallVectorisedWriter{upstream: writer, rawConn: conn}, true
		}
	}
	return nil, false
}

func CreateVectorisedPacketWriter(writer any) (N.VectorisedPacketWriter, bool) {
	switch w := writer.(type) {
	case N.VectorisedPacketWriter:
		return w, true
	case syscall.Conn:
		rawConn, err := w.SyscallConn()
		if err == nil {
			return &SyscallVectorisedPacketWriter{upstream: writer, rawConn: rawConn}, true
		}
	case syscall.RawConn:
		return &SyscallVectorisedPacketWriter{upstream: writer, rawConn: w}, true
	}
	return nil, false
}

var _ N.VectorisedWriter = (*BufferedVectorisedWriter)(nil)

type BufferedVectorisedWriter struct {
	upstream io.Writer
}

func (w *BufferedVectorisedWriter) WriteVectorised(buffers []*buf.Buffer) error {
	defer buf.ReleaseMulti(buffers)
	bufferLen := buf.LenMulti(buffers)
	if bufferLen == 0 {
		return common.Error(w.upstream.Write(nil))
	} else if len(buffers) == 1 {
		return common.Error(w.upstream.Write(buffers[0].Bytes()))
	}
	var bufferBytes []byte
	if bufferLen > 65535 {
		bufferBytes = make([]byte, bufferLen)
	} else {
		buffer := buf.NewSize(bufferLen)
		defer buffer.Release()
		bufferBytes = buffer.FreeBytes()
	}
	buf.CopyMulti(bufferBytes, buffers)
	return common.Error(w.upstream.Write(bufferBytes))
}

func (w *BufferedVectorisedWriter) Upstream() any {
	return w.upstream
}

var _ N.VectorisedWriter = (*NetVectorisedWriterWrapper)(nil)

type NetVectorisedWriterWrapper struct {
	upstream io.Writer
}

func (w *NetVectorisedWriterWrapper) WriteVectorised(buffers []*buf.Buffer) error {
	if buf.LenMulti(buffers) == 0 {
		return os.ErrInvalid
	}
	defer buf.ReleaseMulti(buffers)
	netBuffers := net.Buffers(buf.ToSliceMulti(buffers))
	return common.Error(netBuffers.WriteTo(w.upstream))
}

func (w *NetVectorisedWriterWrapper) Upstream() any {
	return w.upstream
}

func (w *NetVectorisedWriterWrapper) WriterReplaceable() bool {
	return true
}

var _ N.VectorisedWriter = (*SyscallVectorisedWriter)(nil)

type SyscallVectorisedWriter struct {
	upstream any
	rawConn  syscall.RawConn
	syscallVectorisedWriterFields
}

func (w *SyscallVectorisedWriter) Upstream() any {
	return w.upstream
}

func (w *SyscallVectorisedWriter) WriterReplaceable() bool {
	return true
}

var _ N.VectorisedPacketWriter = (*SyscallVectorisedPacketWriter)(nil)

type SyscallVectorisedPacketWriter struct {
	upstream any
	rawConn  syscall.RawConn
	syscallVectorisedWriterFields
}

func (w *SyscallVectorisedPacketWriter) Upstream() any {
	return w.upstream
}

var _ N.VectorisedPacketWriter = (*UnbindVectorisedPacketWriter)(nil)

type UnbindVectorisedPacketWriter struct {
	N.VectorisedWriter
}

func (w *UnbindVectorisedPacketWriter) WriteVectorisedPacket(buffers []*buf.Buffer, _ M.Socksaddr) error {
	return w.WriteVectorised(buffers)
}
