//go:build with_purego

package cronet

import (
	"sync"
	"sync/atomic"

	"github.com/sagernet/cronet-go/internal/cronet"

	"github.com/ebitengine/purego"
)

type runnableEntry struct {
	runFunc   RunnableRunFunc
	destroyed atomic.Bool
}

var (
	runnableAccess     sync.RWMutex
	runnableMap        map[uintptr]*runnableEntry
	runnableCallbackFn uintptr
)

func init() {
	runnableMap = make(map[uintptr]*runnableEntry)
	runnableCallbackFn = purego.NewCallback(runnableRunCallback)
}

func runnableRunCallback(self uintptr) uintptr {
	runnableAccess.RLock()
	entry := runnableMap[self]
	runnableAccess.RUnlock()
	if entry == nil || entry.destroyed.Load() {
		return 0 // Post-destroy callback, silently ignore
	}
	entry.runFunc(Runnable{self})
	// Run is one-shot - safe to cleanup
	runnableAccess.Lock()
	delete(runnableMap, self)
	runnableAccess.Unlock()
	return 0
}

// NewRunnable creates a new Runnable with the given run function.
func NewRunnable(runFunc RunnableRunFunc) Runnable {
	if runFunc == nil {
		panic("nil runnable run function")
	}
	ptr := cronet.RunnableCreateWith(runnableCallbackFn)
	runnableAccess.Lock()
	runnableMap[ptr] = &runnableEntry{runFunc: runFunc}
	runnableAccess.Unlock()
	return Runnable{ptr}
}

func (r Runnable) Destroy() {
	runnableAccess.RLock()
	entry := runnableMap[r.ptr]
	runnableAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
	cronet.RunnableDestroy(r.ptr)
}
