package deadline

import (
	"net"
	"os"
	"sync/atomic"
	"time"

	"github.com/sagernet/sing/common/buf"
	M "github.com/sagernet/sing/common/metadata"
)

type fallbackPacketReader struct {
	*packetReader
	disablePipe atomic.Bool
	inRead      atomic.Bool
}

func NewFallbackPacketReader(timeoutReader TimeoutPacketReader) PacketReader {
	return &fallbackPacketReader{packetReader: NewPacketReader(timeoutReader).(*packetReader)}
}

func (r *fallbackPacketReader) ReadFrom(p []byte) (n int, addr net.Addr, err error) {
	select {
	case result := <-r.result:
		return r.pipeReturnFrom(result, p)
	default:
	}
	select {
	case result := <-r.result:
		return r.pipeReturnFrom(result, p)
	case <-r.pipeDeadline.wait():
		return 0, nil, os.ErrDeadlineExceeded
	case <-r.done:
		if r.disablePipe.Load() {
			return r.TimeoutPacketReader.ReadFrom(p)
		} else if r.deadline.Load().IsZero() {
			r.done <- struct{}{}
			r.inRead.Store(true)
			defer r.inRead.Store(false)
			n, addr, err = r.TimeoutPacketReader.ReadFrom(p)
			return
		}
		go r.pipeReadFrom(len(p))
	}
	select {
	case result := <-r.result:
		return r.pipeReturnFrom(result, p)
	case <-r.pipeDeadline.wait():
		return 0, nil, os.ErrDeadlineExceeded
	}
}

func (r *fallbackPacketReader) ReadPacket(buffer *buf.Buffer) (destination M.Socksaddr, err error) {
	select {
	case result := <-r.result:
		return r.pipeReturnFromBuffer(result, buffer)
	default:
	}
	select {
	case result := <-r.result:
		return r.pipeReturnFromBuffer(result, buffer)
	case <-r.pipeDeadline.wait():
		return M.Socksaddr{}, os.ErrDeadlineExceeded
	case <-r.done:
		if r.disablePipe.Load() {
			return r.TimeoutPacketReader.ReadPacket(buffer)
		} else if r.deadline.Load().IsZero() {
			r.done <- struct{}{}
			r.inRead.Store(true)
			defer r.inRead.Store(false)
			destination, err = r.TimeoutPacketReader.ReadPacket(buffer)
			return
		}
		go r.pipeReadFrom(buffer.FreeLen())
	}
	select {
	case result := <-r.result:
		return r.pipeReturnFromBuffer(result, buffer)
	case <-r.pipeDeadline.wait():
		return M.Socksaddr{}, os.ErrDeadlineExceeded
	}
}

func (r *fallbackPacketReader) SetReadDeadline(t time.Time) error {
	if r.disablePipe.Load() {
		return r.TimeoutPacketReader.SetReadDeadline(t)
	} else if r.inRead.Load() {
		r.disablePipe.Store(true)
		return r.TimeoutPacketReader.SetReadDeadline(t)
	}
	return r.packetReader.SetReadDeadline(t)
}

func (r *fallbackPacketReader) ReaderReplaceable() bool {
	return r.disablePipe.Load() || r.packetReader.ReaderReplaceable()
}

func (r *fallbackPacketReader) UpstreamReader() any {
	return r.packetReader.UpstreamReader()
}
