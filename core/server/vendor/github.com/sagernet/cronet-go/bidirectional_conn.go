package cronet

import (
	"context"
	"io"
	"net"
	"os"
	"sync"
	"time"

	"github.com/sagernet/sing/common/logger"
)

type BidirectionalConn struct {
	ctx              context.Context
	stream           BidirectionalStream
	logger           logger.ContextLogger
	cancelOnce       sync.Once // Ensures Cancel is called at most once
	destroyOnce      sync.Once // Ensures Destroy is called at most once
	readWaitHeaders  bool
	writeWaitHeaders bool
	access           sync.Mutex
	close            chan struct{}
	done             chan struct{}
	err              error
	ready            chan struct{}
	handshake        chan struct{}
	read             chan int
	write            chan struct{}
	headers          map[string]string
	readSemaphore    chan struct{}
	writeSemaphore   chan struct{}
	readBuffer       []byte
	writeBuffer      []byte
	readDone         chan struct{}
	writeDone        chan struct{}
	readDoneOnce     sync.Once
	writeDoneOnce    sync.Once
	trackedConn      *trackedNaiveConn
}

func (e StreamEngine) CreateConn(ctx context.Context, l logger.ContextLogger, readWaitHeaders bool, writeWaitHeaders bool) *BidirectionalConn {
	conn := &BidirectionalConn{
		ctx:              ctx,
		logger:           l,
		readWaitHeaders:  readWaitHeaders,
		writeWaitHeaders: writeWaitHeaders,
		close:            make(chan struct{}),
		done:             make(chan struct{}),
		ready:            make(chan struct{}),
		handshake:        make(chan struct{}),
		read:             make(chan int),
		write:            make(chan struct{}),
		readSemaphore:    make(chan struct{}, 1),
		writeSemaphore:   make(chan struct{}, 1),
		readDone:         make(chan struct{}, 1),
		writeDone:        make(chan struct{}, 1),
	}
	conn.readSemaphore <- struct{}{}
	conn.writeSemaphore <- struct{}{}
	conn.stream = e.CreateStream(&bidirectionalHandler{BidirectionalConn: conn})
	return conn
}

func (c *BidirectionalConn) Start(method string, url string, headers map[string]string, priority int, endOfStream bool) error {
	c.access.Lock()
	defer c.access.Unlock()
	select {
	case <-c.close:
		return net.ErrClosed
	case <-c.done:
		return net.ErrClosed
	default:
	}
	if !c.stream.Start(method, url, headers, priority, endOfStream) {
		return os.ErrInvalid
	}
	return nil
}

func (c *BidirectionalConn) Read(p []byte) (n int, err error) {
	select {
	case <-c.close:
		return 0, net.ErrClosed
	case <-c.done:
		return 0, net.ErrClosed
	default:
	}
	if len(p) == 0 {
		return 0, nil
	}

	select {
	case <-c.close:
		return 0, net.ErrClosed
	case <-c.done:
		return 0, net.ErrClosed
	case <-c.readSemaphore:
	}
	defer func() { c.readSemaphore <- struct{}{} }()

	if c.readWaitHeaders {
		select {
		case <-c.handshake:
			break
		case <-c.done:
			return 0, c.err
		}
	} else {
		select {
		case <-c.ready:
			break
		case <-c.done:
			return 0, c.err
		}
	}

	c.access.Lock()

	select {
	case <-c.close:
		c.access.Unlock()
		return 0, net.ErrClosed
	case <-c.done:
		c.access.Unlock()
		return 0, net.ErrClosed
	default:
	}

	if len(c.readBuffer) < len(p) {
		c.readBuffer = make([]byte, len(p))
	}
	readBuffer := c.readBuffer[:len(p)]
	c.stream.Read(readBuffer)
	c.access.Unlock()

	select {
	case bytesRead := <-c.read:
		if bytesRead > len(p) {
			bytesRead = len(p)
		}
		if bytesRead > 0 {
			copy(p, readBuffer[:bytesRead])
		}
		return bytesRead, nil
	case <-c.done:
		<-c.readDone
		return 0, c.err
	case <-c.close:
		<-c.readDone
		return 0, net.ErrClosed
	}
}

func (c *BidirectionalConn) Write(p []byte) (n int, err error) {
	select {
	case <-c.close:
		return 0, net.ErrClosed
	case <-c.done:
		return 0, net.ErrClosed
	default:
	}
	if len(p) == 0 {
		return 0, nil
	}

	select {
	case <-c.close:
		return 0, net.ErrClosed
	case <-c.done:
		return 0, net.ErrClosed
	case <-c.writeSemaphore:
	}
	defer func() { c.writeSemaphore <- struct{}{} }()

	if c.writeWaitHeaders {
		select {
		case <-c.handshake:
			break
		case <-c.done:
			return 0, c.err
		}
	} else {
		select {
		case <-c.ready:
			break
		case <-c.done:
			return 0, c.err
		}
	}

	c.access.Lock()

	select {
	case <-c.close:
		c.access.Unlock()
		return 0, net.ErrClosed
	case <-c.done:
		c.access.Unlock()
		return 0, net.ErrClosed
	default:
	}

	if len(c.writeBuffer) < len(p) {
		c.writeBuffer = make([]byte, len(p))
	}
	writeBuffer := c.writeBuffer[:len(p)]
	copy(writeBuffer, p)
	c.stream.Write(writeBuffer, false)
	c.access.Unlock()

	select {
	case <-c.write:
		return len(p), nil
	case <-c.done:
		<-c.writeDone
		return 0, c.err
	case <-c.close:
		<-c.writeDone
		return 0, net.ErrClosed
	}
}

func (c *BidirectionalConn) Done() <-chan struct{} {
	return c.done
}

func (c *BidirectionalConn) bindTrackedNaiveConn(tracked *trackedNaiveConn) {
	if tracked == nil {
		return
	}

	c.access.Lock()
	select {
	case <-c.done:
		c.access.Unlock()
		tracked.release()
		return
	default:
	}
	c.trackedConn = tracked
	c.access.Unlock()
}

func (c *BidirectionalConn) Err() error {
	return c.err
}

func (c *BidirectionalConn) Close() error {
	c.access.Lock()

	select {
	case <-c.close:
		c.access.Unlock()
		return net.ErrClosed
	case <-c.done:
		c.access.Unlock()
		return nil // Stream already terminated normally
	default:
	}

	close(c.close)
	c.access.Unlock()

	c.cancelOnce.Do(func() {
		c.stream.Cancel()
	})
	return nil
}

func (c *BidirectionalConn) signalReadDone() {
	c.readDoneOnce.Do(func() { close(c.readDone) })
}

func (c *BidirectionalConn) signalWriteDone() {
	c.writeDoneOnce.Do(func() { close(c.writeDone) })
}

func (c *BidirectionalConn) LocalAddr() net.Addr {
	return nil
}

func (c *BidirectionalConn) RemoteAddr() net.Addr {
	return nil
}

func (c *BidirectionalConn) NeedAdditionalReadDeadline() bool {
	return true
}

func (c *BidirectionalConn) SetDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *BidirectionalConn) SetReadDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *BidirectionalConn) SetWriteDeadline(t time.Time) error {
	return os.ErrInvalid
}

func (c *BidirectionalConn) WaitForHeaders() (map[string]string, error) {
	select {
	case <-c.close:
		return nil, net.ErrClosed
	case <-c.done:
		return nil, net.ErrClosed
	default:
	}

	select {
	case <-c.handshake:
		return c.headers, nil
	case <-c.done:
		return nil, c.err
	}
}

func (c *BidirectionalConn) WaitForHeadersContext(ctx context.Context) (map[string]string, error) {
	select {
	case <-c.close:
		return nil, net.ErrClosed
	case <-c.done:
		return nil, net.ErrClosed
	default:
	}

	select {
	case <-ctx.Done():
		return nil, ctx.Err()
	case <-c.handshake:
		return c.headers, nil
	case <-c.done:
		return nil, c.err
	case <-c.close:
		return nil, net.ErrClosed
	}
}

type bidirectionalHandler struct {
	*BidirectionalConn
	readyOnce     sync.Once
	handshakeOnce sync.Once
	doneOnce      sync.Once
}

func (c *bidirectionalHandler) OnStreamReady(stream BidirectionalStream) {
	c.readyOnce.Do(func() { close(c.ready) })
}

func (c *bidirectionalHandler) OnResponseHeadersReceived(stream BidirectionalStream, headers map[string]string, negotiatedProtocol string) {
	c.headers = headers
	c.logger.DebugContext(c.ctx, "response received, protocol: ", negotiatedProtocol, ", status: ", headers[":status"])
	c.handshakeOnce.Do(func() { close(c.handshake) })
}

func (c *bidirectionalHandler) OnReadCompleted(stream BidirectionalStream, bytesRead int) {
	c.access.Lock()

	if c.err != nil {
		c.access.Unlock()
		c.signalReadDone()
		return
	}

	if bytesRead == 0 {
		c.access.Unlock()
		c.signalReadDone()
		c.signalWriteDone()
		c.cancelOnce.Do(func() {})
		c.Close(io.EOF)
		return
	}

	c.access.Unlock()

	select {
	case <-c.close:
		c.signalReadDone()
	case <-c.done:
		c.signalReadDone()
	case c.read <- bytesRead:
	}
}

func (c *bidirectionalHandler) OnWriteCompleted(stream BidirectionalStream) {
	c.access.Lock()

	if c.err != nil {
		c.access.Unlock()
		c.signalWriteDone()
		return
	}

	c.access.Unlock()

	select {
	case <-c.close:
		c.signalWriteDone()
	case <-c.done:
		c.signalWriteDone()
	case c.write <- struct{}{}:
	}
}

func (c *bidirectionalHandler) OnResponseTrailersReceived(stream BidirectionalStream, trailers map[string]string) {
}

func (c *bidirectionalHandler) OnSucceeded(stream BidirectionalStream) {
	c.signalReadDone()
	c.signalWriteDone()
	c.cancelOnce.Do(func() {})
	c.Close(io.EOF)
}

func (c *bidirectionalHandler) OnFailed(stream BidirectionalStream, netError int) {
	c.logger.WarnContext(c.ctx, "stream failed: ", NetError(netError))
	c.signalReadDone()
	c.signalWriteDone()
	c.cancelOnce.Do(func() {})
	c.Close(NetError(netError))
}

func (c *bidirectionalHandler) OnCanceled(stream BidirectionalStream) {
	c.logger.DebugContext(c.ctx, "stream canceled")
	c.signalReadDone()
	c.signalWriteDone()
	c.cancelOnce.Do(func() {})
	c.Close(context.Canceled)
}

func (c *bidirectionalHandler) Close(err error) {
	c.doneOnce.Do(func() {
		var tracked *trackedNaiveConn
		c.access.Lock()
		c.err = err
		close(c.done)
		tracked = c.trackedConn
		c.access.Unlock()

		if tracked != nil {
			tracked.release()
		}

		c.destroyOnce.Do(func() {
			c.stream.Destroy()
		})
	})
}
