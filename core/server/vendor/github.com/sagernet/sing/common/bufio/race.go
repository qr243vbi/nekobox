package bufio

import (
	"io"
	"sync"

	"github.com/sagernet/sing/common/buf"
	N "github.com/sagernet/sing/common/network"
)

type RaceWriter struct {
	upstream N.ExtendedWriter
	access   sync.Mutex
}

func NewRaceWriter(writer io.Writer) *RaceWriter {
	return &RaceWriter{
		upstream: NewExtendedWriter(writer),
	}
}

func (w *RaceWriter) WriteBuffer(buffer *buf.Buffer) error {
	w.access.Lock()
	defer w.access.Unlock()
	return w.upstream.WriteBuffer(buffer)
}

func (w *RaceWriter) Write(p []byte) (n int, err error) {
	w.access.Lock()
	defer w.access.Unlock()
	return w.upstream.Write(p)
}

func (w *RaceWriter) Upstream() any {
	return w.upstream
}
