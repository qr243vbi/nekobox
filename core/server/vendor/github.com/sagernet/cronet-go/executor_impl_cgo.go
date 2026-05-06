//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT void cronetExecutorExecute(Cronet_ExecutorPtr self,Cronet_RunnablePtr command);
import "C"

import (
	"sync"
	"sync/atomic"
	"unsafe"
)

type executorEntry struct {
	executeFunc ExecutorExecuteFunc
	destroyed   atomic.Bool
}

var (
	executorAccess sync.RWMutex
	executors      map[uintptr]*executorEntry
)

func init() {
	executors = make(map[uintptr]*executorEntry)
}

func NewExecutor(executeFunc ExecutorExecuteFunc) Executor {
	if executeFunc == nil {
		panic("nil executor execute function")
	}
	ptr := C.Cronet_Executor_CreateWith((*[0]byte)(C.cronetExecutorExecute))
	ptrVal := uintptr(unsafe.Pointer(ptr))
	executorAccess.Lock()
	executors[ptrVal] = &executorEntry{executeFunc: executeFunc}
	executorAccess.Unlock()
	return Executor{ptrVal}
}

func (e Executor) Destroy() {
	executorAccess.Lock()
	entry := executors[e.ptr]
	if entry != nil {
		entry.destroyed.Store(true)
	}
	executorAccess.Unlock()
	C.Cronet_Executor_Destroy(C.Cronet_ExecutorPtr(unsafe.Pointer(e.ptr)))
	// Cleanup after C destroy
	executorAccess.Lock()
	delete(executors, e.ptr)
	executorAccess.Unlock()
}

//export cronetExecutorExecute
func cronetExecutorExecute(self C.Cronet_ExecutorPtr, command C.Cronet_RunnablePtr) {
	executorAccess.RLock()
	entry := executors[uintptr(unsafe.Pointer(self))]
	executorAccess.RUnlock()
	if entry == nil || entry.destroyed.Load() {
		return // Post-destroy callback, silently ignore
	}
	entry.executeFunc(Executor{uintptr(unsafe.Pointer(self))}, Runnable{uintptr(unsafe.Pointer(command))})
}
