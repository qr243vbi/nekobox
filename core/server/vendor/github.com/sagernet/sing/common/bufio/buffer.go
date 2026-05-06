package bufio

import (
	"io"
	"sync"

	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/buf"
)

type BufferedWriter struct {
	upstream io.Writer
	buffer   *buf.Buffer
	access   sync.Mutex
}

func NewBufferedWriter(upstream io.Writer, buffer *buf.Buffer) *BufferedWriter {
	return &BufferedWriter{
		upstream: upstream,
		buffer:   buffer,
	}
}

func (w *BufferedWriter) Write(p []byte) (n int, err error) {
	w.access.Lock()
	defer w.access.Unlock()
	if w.buffer == nil {
		return w.upstream.Write(p)
	}
	for {
		var writeN int
		writeN, err = w.buffer.Write(p[n:])
		n += writeN
		if n == len(p) {
			return
		}
		_, err = w.upstream.Write(w.buffer.Bytes())
		if err != nil {
			return
		}
		w.buffer.Reset()
	}
}

func (w *BufferedWriter) WriteByte(c byte) error {
	w.access.Lock()
	defer w.access.Unlock()
	if w.buffer == nil {
		return common.Error(w.upstream.Write([]byte{c}))
	}
	for {
		err := w.buffer.WriteByte(c)
		if err == nil {
			return nil
		}
		_, err = w.upstream.Write(w.buffer.Bytes())
		if err != nil {
			return err
		}
		w.buffer.Reset()
	}
}

func (w *BufferedWriter) Fallthrough() error {
	w.access.Lock()
	defer w.access.Unlock()
	if w.buffer == nil {
		return nil
	}
	if !w.buffer.IsEmpty() {
		_, err := w.upstream.Write(w.buffer.Bytes())
		if err != nil {
			return err
		}
	}
	w.buffer.Release()
	w.buffer = nil
	return nil
}

func (w *BufferedWriter) WriterReplaceable() bool {
	return w.buffer == nil
}
