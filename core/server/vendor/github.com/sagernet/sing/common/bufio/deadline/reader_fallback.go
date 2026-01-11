package deadline

import (
	"os"
	"sync/atomic"
	"time"

	"github.com/sagernet/sing/common/buf"
)

type fallbackReader struct {
	*reader
	disablePipe atomic.Bool
	inRead      atomic.Bool
}

func NewFallbackReader(timeoutReader TimeoutReader) Reader {
	return &fallbackReader{reader: NewReader(timeoutReader).(*reader)}
}

func (r *fallbackReader) Read(p []byte) (n int, err error) {
	select {
	case result := <-r.result:
		return r.pipeReturn(result, p)
	default:
	}
	select {
	case result := <-r.result:
		return r.pipeReturn(result, p)
	case <-r.pipeDeadline.wait():
		return 0, os.ErrDeadlineExceeded
	case <-r.done:
		if r.disablePipe.Load() {
			return r.ExtendedReader.Read(p)
		} else if r.deadline.Load().IsZero() {
			r.done <- struct{}{}
			r.inRead.Store(true)
			defer r.inRead.Store(false)
			n, err = r.ExtendedReader.Read(p)
			return
		}
		go r.pipeRead(len(p))
	}
	select {
	case result := <-r.result:
		return r.pipeReturn(result, p)
	case <-r.pipeDeadline.wait():
		return 0, os.ErrDeadlineExceeded
	}
}

func (r *fallbackReader) ReadBuffer(buffer *buf.Buffer) error {
	select {
	case result := <-r.result:
		return r.pipeReturnBuffer(result, buffer)
	default:
	}
	select {
	case result := <-r.result:
		return r.pipeReturnBuffer(result, buffer)
	case <-r.pipeDeadline.wait():
		return os.ErrDeadlineExceeded
	case <-r.done:
		if r.disablePipe.Load() {
			return r.ExtendedReader.ReadBuffer(buffer)
		} else if r.deadline.Load().IsZero() {
			r.done <- struct{}{}
			r.inRead.Store(true)
			defer r.inRead.Store(false)
			return r.ExtendedReader.ReadBuffer(buffer)
		}
		go r.pipeRead(buffer.FreeLen())
	}
	select {
	case result := <-r.result:
		return r.pipeReturnBuffer(result, buffer)
	case <-r.pipeDeadline.wait():
		return os.ErrDeadlineExceeded
	}
}

func (r *fallbackReader) SetReadDeadline(t time.Time) error {
	if r.disablePipe.Load() {
		return r.timeoutReader.SetReadDeadline(t)
	} else if r.inRead.Load() {
		r.disablePipe.Store(true)
		return r.timeoutReader.SetReadDeadline(t)
	}
	return r.reader.SetReadDeadline(t)
}

func (r *fallbackReader) ReaderReplaceable() bool {
	return r.disablePipe.Load() || r.reader.ReaderReplaceable()
}

func (r *fallbackReader) UpstreamReader() any {
	return r.reader.UpstreamReader()
}
