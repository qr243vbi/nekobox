package pipe

import (
	"io"
	"net"
	"os"

	"github.com/sagernet/sing/common/buf"
	N "github.com/sagernet/sing/common/network"
)

var _ N.ReadWaiter = (*pipe)(nil)

func (p *pipe) InitializeReadWaiter(options N.ReadWaitOptions) (needCopy bool) {
	p.readWaitOptions = options
	return false
}

func (p *pipe) WaitReadBuffer() (buffer *buf.Buffer, err error) {
	buffer, err = p.waitReadBuffer()
	if err != nil && err != io.EOF && err != io.ErrClosedPipe {
		err = &net.OpError{Op: "read", Net: "pipe", Err: err}
	}
	return
}

func (p *pipe) waitReadBuffer() (buffer *buf.Buffer, err error) {
	switch {
	case isClosedChan(p.localDone):
		return nil, io.ErrClosedPipe
	case isClosedChan(p.remoteDone):
		return nil, io.EOF
	case isClosedChan(p.readDeadline.Wait()):
		return nil, os.ErrDeadlineExceeded
	}
	select {
	case bw := <-p.rdRx:
		buffer = p.readWaitOptions.NewBuffer()
		var nr int
		nr, err = buffer.Write(bw)
		if err != nil {
			buffer.Release()
			return
		}
		p.readWaitOptions.PostReturn(buffer)
		p.rdTx <- nr
		return
	case <-p.localDone:
		return nil, io.ErrClosedPipe
	case <-p.remoteDone:
		return nil, io.EOF
	case <-p.readDeadline.Wait():
		return nil, os.ErrDeadlineExceeded
	}
}
