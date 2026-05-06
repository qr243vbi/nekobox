package canceler

import (
	"context"
	"net"
	"os"
	"time"

	"github.com/sagernet/sing/common"
)

type Instance struct {
	ctx        context.Context
	cancelFunc common.ContextCancelCauseFunc
	timer      *time.Timer
	timeout    time.Duration
}

func New(ctx context.Context, cancelFunc common.ContextCancelCauseFunc, timeout time.Duration) *Instance {
	instance := &Instance{
		ctx,
		cancelFunc,
		time.NewTimer(timeout),
		timeout,
	}
	go instance.wait()
	return instance
}

func (i *Instance) Update() bool {
	if !i.timer.Stop() {
		return false
	}
	if !i.timer.Reset(i.timeout) {
		return false
	}
	return true
}

func (i *Instance) Timeout() time.Duration {
	return i.timeout
}

func (i *Instance) SetTimeout(timeout time.Duration) bool {
	i.timeout = timeout
	return i.Update()
}

func (i *Instance) wait() {
	select {
	case <-i.timer.C:
	case <-i.ctx.Done():
	}
	i.CloseWithError(os.ErrDeadlineExceeded)
}

func (i *Instance) Close() error {
	i.CloseWithError(net.ErrClosed)
	return nil
}

func (i *Instance) CloseWithError(err error) {
	i.timer.Stop()
	i.cancelFunc(err)
}
