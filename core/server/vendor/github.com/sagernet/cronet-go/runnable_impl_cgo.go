//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT void cronetRunnableRun(Cronet_RunnablePtr self);
import "C"

import (
	"sync"
	"sync/atomic"
	"unsafe"
)

type runnableEntry struct {
	runFunc   RunnableRunFunc
	destroyed atomic.Bool
}

var (
	runnableAccess sync.RWMutex
	runnableMap    map[uintptr]*runnableEntry
)

func init() {
	runnableMap = make(map[uintptr]*runnableEntry)
}

// NewRunnable creates a new Runnable with the given run function.
func NewRunnable(runFunc RunnableRunFunc) Runnable {
	if runFunc == nil {
		panic("nil runnable run function")
	}
	ptr := C.Cronet_Runnable_CreateWith((*[0]byte)(C.cronetRunnableRun))
	ptrVal := uintptr(unsafe.Pointer(ptr))
	runnableAccess.Lock()
	runnableMap[ptrVal] = &runnableEntry{runFunc: runFunc}
	runnableAccess.Unlock()
	return Runnable{ptrVal}
}

func (r Runnable) Destroy() {
	runnableAccess.RLock()
	entry := runnableMap[r.ptr]
	runnableAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
	C.Cronet_Runnable_Destroy(C.Cronet_RunnablePtr(unsafe.Pointer(r.ptr)))
}

//export cronetRunnableRun
func cronetRunnableRun(self C.Cronet_RunnablePtr) {
	ptr := uintptr(unsafe.Pointer(self))
	runnableAccess.RLock()
	entry := runnableMap[ptr]
	runnableAccess.RUnlock()
	if entry == nil || entry.destroyed.Load() {
		return // Post-destroy callback, silently ignore
	}
	entry.runFunc(Runnable{ptr})
	// Run is one-shot - safe to cleanup
	runnableAccess.Lock()
	delete(runnableMap, ptr)
	runnableAccess.Unlock()
}
