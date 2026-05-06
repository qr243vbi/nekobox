//go:build with_purego

package cronet

// Design Philosophy: Fail-Fast
//
// This file intentionally does NOT use panic recovery in callbacks.
// If a callback handler panics, the process should crash immediately.
// Using recover() would mask programming errors and make debugging harder.
// Errors should be visible and cause immediate failure, not be silently swallowed.
//
// Note: Post-destroy callbacks silently return because they are expected
// async API behavior, not programming errors.

import (
	"sync"
	"sync/atomic"

	"github.com/sagernet/cronet-go/internal/cronet"

	"github.com/ebitengine/purego"
)

type executorEntry struct {
	executeFunc ExecutorExecuteFunc
	destroyed   atomic.Bool
}

var (
	executorAccess     sync.RWMutex
	executors          map[uintptr]*executorEntry
	executorCallbackFn uintptr
)

func init() {
	executors = make(map[uintptr]*executorEntry)
	executorCallbackFn = purego.NewCallback(executorExecuteCallback)
}

func executorExecuteCallback(self, command uintptr) uintptr {
	executorAccess.RLock()
	entry := executors[self]
	executorAccess.RUnlock()
	if entry == nil || entry.destroyed.Load() {
		return 0 // Post-destroy callback, silently ignore
	}
	entry.executeFunc(Executor{self}, Runnable{command})
	return 0
}

func NewExecutor(executeFunc ExecutorExecuteFunc) Executor {
	if executeFunc == nil {
		panic("nil executor execute function")
	}
	ptr := cronet.ExecutorCreateWith(executorCallbackFn)
	executorAccess.Lock()
	executors[ptr] = &executorEntry{executeFunc: executeFunc}
	executorAccess.Unlock()
	return Executor{ptr}
}

func (e Executor) Destroy() {
	executorAccess.Lock()
	entry := executors[e.ptr]
	if entry != nil {
		entry.destroyed.Store(true)
	}
	executorAccess.Unlock()
	cronet.ExecutorDestroy(e.ptr)
	// Cleanup after C destroy
	executorAccess.Lock()
	delete(executors, e.ptr)
	executorAccess.Unlock()
}
