package transport

import (
	"context"
	"net"
	"sync"
	"time"

	"github.com/sagernet/sing/common/x/list"
)

type ConnPoolMode int

const (
	ConnPoolSingle ConnPoolMode = iota
	ConnPoolOrdered
)

type ConnPoolOptions[T comparable] struct {
	Mode    ConnPoolMode
	IsAlive func(T) bool
	Close   func(T, error)
}

type ConnPool[T comparable] struct {
	options ConnPoolOptions[T]

	access sync.Mutex
	closed bool
	state  *connPoolState[T]
}

type connPoolState[T comparable] struct {
	ctx    context.Context
	cancel context.CancelCauseFunc

	all map[T]struct{}

	idle         list.List[T]
	idleElements map[T]*list.Element[T]

	shared        T
	hasShared     bool
	sharedClaimed bool
	sharedCtx     context.Context
	sharedCancel  context.CancelCauseFunc

	connecting *connPoolConnect[T]
}

type connPoolConnect[T comparable] struct {
	done chan struct{}
	err  error
}

type connPoolDialContext struct {
	context.Context
	parent context.Context
}

func (c connPoolDialContext) Deadline() (time.Time, bool) {
	return c.parent.Deadline()
}

func (c connPoolDialContext) Value(key any) any {
	return c.parent.Value(key)
}

func NewConnPool[T comparable](options ConnPoolOptions[T]) *ConnPool[T] {
	return &ConnPool[T]{
		options: options,
		state:   newConnPoolState[T](options.Mode),
	}
}

func newConnPoolState[T comparable](mode ConnPoolMode) *connPoolState[T] {
	ctx, cancel := context.WithCancelCause(context.Background())
	state := &connPoolState[T]{
		ctx:    ctx,
		cancel: cancel,
		all:    make(map[T]struct{}),
	}
	if mode == ConnPoolOrdered {
		state.idleElements = make(map[T]*list.Element[T])
	}
	return state
}

func (p *ConnPool[T]) Acquire(ctx context.Context, dial func(context.Context) (T, error)) (T, bool, error) {
	switch p.options.Mode {
	case ConnPoolSingle:
		conn, _, created, err := p.acquireShared(ctx, dial)
		return conn, created, err
	case ConnPoolOrdered:
		return p.acquireOrdered(ctx, dial)
	default:
		var zero T
		return zero, false, net.ErrClosed
	}
}

func (p *ConnPool[T]) AcquireShared(ctx context.Context, dial func(context.Context) (T, error)) (T, context.Context, bool, error) {
	if p.options.Mode != ConnPoolSingle {
		var zero T
		return zero, nil, false, net.ErrClosed
	}
	return p.acquireShared(ctx, dial)
}

func (p *ConnPool[T]) Release(conn T, reuse bool) {
	var (
		closeConn bool
		closeErr  error
	)

	p.access.Lock()
	if p.closed || p.state == nil {
		closeConn = true
		closeErr = net.ErrClosed
		p.access.Unlock()
		if closeConn {
			p.options.Close(conn, closeErr)
		}
		return
	}

	currentState := p.state
	_, tracked := currentState.all[conn]
	if !tracked {
		closeConn = true
		closeErr = p.closeCause(currentState)
		p.access.Unlock()
		if closeConn {
			p.options.Close(conn, closeErr)
		}
		return
	}

	if !reuse || !p.options.IsAlive(conn) {
		delete(currentState.all, conn)
		switch p.options.Mode {
		case ConnPoolSingle:
			if currentState.hasShared && currentState.shared == conn {
				var zero T
				currentState.shared = zero
				currentState.hasShared = false
				currentState.sharedClaimed = false
				currentState.sharedCtx = nil
				if currentState.sharedCancel != nil {
					currentState.sharedCancel(net.ErrClosed)
					currentState.sharedCancel = nil
				}
			}
		case ConnPoolOrdered:
			if element, loaded := currentState.idleElements[conn]; loaded {
				currentState.idle.Remove(element)
				delete(currentState.idleElements, conn)
			}
		}
		closeConn = true
		closeErr = net.ErrClosed
		p.access.Unlock()
		if closeConn {
			p.options.Close(conn, closeErr)
		}
		return
	}

	if p.options.Mode == ConnPoolOrdered {
		if _, loaded := currentState.idleElements[conn]; !loaded {
			currentState.idleElements[conn] = currentState.idle.PushBack(conn)
		}
	}
	p.access.Unlock()
}

func (p *ConnPool[T]) Invalidate(conn T, cause error) {
	p.access.Lock()
	if p.closed || p.state == nil {
		p.access.Unlock()
		p.options.Close(conn, cause)
		return
	}

	currentState := p.state
	_, tracked := currentState.all[conn]
	if !tracked {
		p.access.Unlock()
		return
	}

	delete(currentState.all, conn)
	switch p.options.Mode {
	case ConnPoolSingle:
		if currentState.hasShared && currentState.shared == conn {
			var zero T
			currentState.shared = zero
			currentState.hasShared = false
			currentState.sharedClaimed = false
			currentState.sharedCtx = nil
			if currentState.sharedCancel != nil {
				currentState.sharedCancel(cause)
				currentState.sharedCancel = nil
			}
		}
	case ConnPoolOrdered:
		if element, loaded := currentState.idleElements[conn]; loaded {
			currentState.idle.Remove(element)
			delete(currentState.idleElements, conn)
		}
	}
	p.access.Unlock()

	p.options.Close(conn, cause)
}

func (p *ConnPool[T]) Reset() {
	p.access.Lock()
	if p.closed {
		p.access.Unlock()
		return
	}

	oldState := p.state
	p.state = newConnPoolState[T](p.options.Mode)
	p.access.Unlock()

	p.closeState(oldState, net.ErrClosed)
}

func (p *ConnPool[T]) Close() error {
	p.access.Lock()
	if p.closed {
		p.access.Unlock()
		return nil
	}

	p.closed = true
	oldState := p.state
	p.state = nil
	p.access.Unlock()

	p.closeState(oldState, net.ErrClosed)
	return nil
}

func (p *ConnPool[T]) acquireOrdered(ctx context.Context, dial func(context.Context) (T, error)) (T, bool, error) {
	var zero T
	for {
		var (
			staleConn T
			hasStale  bool
		)

		p.access.Lock()
		if p.closed {
			p.access.Unlock()
			return zero, false, net.ErrClosed
		}

		currentState := p.state
		if element := currentState.idle.Front(); element != nil {
			conn := currentState.idle.Remove(element)
			delete(currentState.idleElements, conn)
			if p.options.IsAlive(conn) {
				p.access.Unlock()
				return conn, false, nil
			}
			delete(currentState.all, conn)
			staleConn = conn
			hasStale = true
		}
		p.access.Unlock()

		if hasStale {
			p.options.Close(staleConn, net.ErrClosed)
			continue
		}

		conn, err := p.dial(ctx, currentState, dial)
		if err != nil {
			return zero, false, err
		}

		p.access.Lock()
		if p.closed {
			p.access.Unlock()
			p.options.Close(conn, net.ErrClosed)
			return zero, false, net.ErrClosed
		}
		if p.state != currentState {
			cause := p.closeCause(currentState)
			p.access.Unlock()
			p.options.Close(conn, cause)
			return zero, false, cause
		}
		currentState.all[conn] = struct{}{}
		p.access.Unlock()
		return conn, true, nil
	}
}

func (p *ConnPool[T]) acquireShared(ctx context.Context, dial func(context.Context) (T, error)) (T, context.Context, bool, error) {
	var zero T
	for {
		var (
			staleConn T
			hasStale  bool
			state     *connPoolConnect[T]
			current   *connPoolState[T]
			startDial bool
		)

		p.access.Lock()
		if p.closed {
			p.access.Unlock()
			return zero, nil, false, net.ErrClosed
		}

		current = p.state
		if current.hasShared {
			conn := current.shared
			if p.options.IsAlive(conn) {
				created := !current.sharedClaimed
				current.sharedClaimed = true
				connCtx := current.sharedCtx
				p.access.Unlock()
				return conn, connCtx, created, nil
			}
			delete(current.all, conn)
			var zeroConn T
			current.shared = zeroConn
			current.hasShared = false
			current.sharedClaimed = false
			current.sharedCtx = nil
			if current.sharedCancel != nil {
				current.sharedCancel(net.ErrClosed)
				current.sharedCancel = nil
			}
			staleConn = conn
			hasStale = true
			p.access.Unlock()
			p.options.Close(staleConn, net.ErrClosed)
			continue
		}

		if current.connecting == nil {
			current.connecting = &connPoolConnect[T]{
				done: make(chan struct{}),
			}
			startDial = true
		}
		state = current.connecting
		p.access.Unlock()

		if hasStale {
			continue
		}
		if startDial {
			go p.connectSingle(current, state, ctx, dial)
		}

		select {
		case <-state.done:
			conn, connCtx, created, retry, err := p.collectShared(current, state, startDial)
			if retry {
				continue
			}
			return conn, connCtx, created, err
		case <-ctx.Done():
			return zero, nil, false, ctx.Err()
		case <-current.ctx.Done():
			p.access.Lock()
			closed := p.closed
			p.access.Unlock()
			if closed {
				return zero, nil, false, net.ErrClosed
			}
		}
	}
}

func (p *ConnPool[T]) connectSingle(current *connPoolState[T], state *connPoolConnect[T], ctx context.Context, dial func(context.Context) (T, error)) {
	conn, err := p.dial(ctx, current, dial)
	if err != nil {
		p.access.Lock()
		if current.connecting == state {
			current.connecting = nil
		}
		state.err = err
		p.access.Unlock()
		close(state.done)
		return
	}

	var closeErr error

	p.access.Lock()
	if current.connecting == state {
		current.connecting = nil
	}
	if p.closed {
		closeErr = net.ErrClosed
		state.err = closeErr
	} else if p.state != current {
		closeErr = p.closeCause(current)
		state.err = closeErr
	} else {
		sharedCtx, sharedCancel := context.WithCancelCause(current.ctx)
		current.shared = conn
		current.hasShared = true
		current.sharedClaimed = false
		current.sharedCtx = sharedCtx
		current.sharedCancel = sharedCancel
		current.all[conn] = struct{}{}
	}
	p.access.Unlock()

	if closeErr != nil {
		p.options.Close(conn, closeErr)
	}
	close(state.done)
}

func (p *ConnPool[T]) collectShared(current *connPoolState[T], state *connPoolConnect[T], startDial bool) (T, context.Context, bool, bool, error) {
	var zero T

	p.access.Lock()
	if state.err != nil {
		err := state.err
		p.access.Unlock()
		if startDial {
			return zero, nil, false, false, err
		}
		return zero, nil, false, true, nil
	}
	if p.closed {
		p.access.Unlock()
		return zero, nil, false, false, net.ErrClosed
	}
	if p.state != current {
		cause := p.closeCause(current)
		p.access.Unlock()
		return zero, nil, false, false, cause
	}
	if !current.hasShared {
		p.access.Unlock()
		return zero, nil, false, true, nil
	}

	conn := current.shared
	if !p.options.IsAlive(conn) {
		delete(current.all, conn)
		var zeroConn T
		current.shared = zeroConn
		current.hasShared = false
		current.sharedClaimed = false
		current.sharedCtx = nil
		if current.sharedCancel != nil {
			current.sharedCancel(net.ErrClosed)
			current.sharedCancel = nil
		}
		p.access.Unlock()
		p.options.Close(conn, net.ErrClosed)
		return zero, nil, false, true, nil
	}

	created := !current.sharedClaimed
	current.sharedClaimed = true
	connCtx := current.sharedCtx
	p.access.Unlock()
	return conn, connCtx, created, false, nil
}

func (p *ConnPool[T]) dial(ctx context.Context, current *connPoolState[T], dial func(context.Context) (T, error)) (T, error) {
	var zero T

	if err := ctx.Err(); err != nil {
		return zero, err
	}
	if cause := context.Cause(current.ctx); cause != nil {
		return zero, cause
	}

	dialCtx, cancel := context.WithCancelCause(current.ctx)
	var (
		stateAccess  sync.Mutex
		dialComplete bool
	)
	stopCancel := context.AfterFunc(ctx, func() {
		stateAccess.Lock()
		if !dialComplete {
			cancel(context.Cause(ctx))
		}
		stateAccess.Unlock()
	})

	select {
	case <-ctx.Done():
		stateAccess.Lock()
		dialComplete = true
		stateAccess.Unlock()
		stopCancel()
		cancel(context.Cause(ctx))
		return zero, ctx.Err()
	default:
	}

	conn, err := dial(connPoolDialContext{
		Context: dialCtx,
		parent:  ctx,
	})
	stateAccess.Lock()
	dialComplete = true
	stateAccess.Unlock()
	stopCancel()
	if err != nil {
		if cause := context.Cause(dialCtx); cause != nil {
			return zero, cause
		}
		return zero, err
	}
	if cause := context.Cause(dialCtx); cause != nil {
		p.options.Close(conn, cause)
		return zero, cause
	}
	return conn, nil
}

func (p *ConnPool[T]) closeState(state *connPoolState[T], cause error) {
	if state == nil {
		return
	}

	state.cancel(cause)
	if state.sharedCancel != nil {
		state.sharedCancel(cause)
	}
	for conn := range state.all {
		p.options.Close(conn, cause)
	}
}

func (p *ConnPool[T]) closeCause(state *connPoolState[T]) error {
	_ = state
	return net.ErrClosed
}
