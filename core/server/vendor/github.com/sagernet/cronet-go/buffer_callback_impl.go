//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT void cronetBufferCallbackOnDestroy(Cronet_BufferCallbackPtr self,Cronet_BufferPtr buffer);
import "C"

import (
	"sync"
	"sync/atomic"
	"unsafe"
)

type bufferCallbackEntry struct {
	callback  BufferCallbackFunc
	destroyed atomic.Bool
}

var (
	bufferCallbackAccess sync.RWMutex
	bufferCallbackMap    map[uintptr]*bufferCallbackEntry
)

func init() {
	bufferCallbackMap = make(map[uintptr]*bufferCallbackEntry)
}

func NewBufferCallback(callbackFunc BufferCallbackFunc) BufferCallback {
	ptr := C.Cronet_BufferCallback_CreateWith((*[0]byte)(C.cronetBufferCallbackOnDestroy))
	ptrVal := uintptr(unsafe.Pointer(ptr))
	if callbackFunc != nil {
		bufferCallbackAccess.Lock()
		bufferCallbackMap[ptrVal] = &bufferCallbackEntry{callback: callbackFunc}
		bufferCallbackAccess.Unlock()
	}
	return BufferCallback{ptrVal}
}

func (c BufferCallback) destroy() {
	bufferCallbackAccess.RLock()
	entry := bufferCallbackMap[c.ptr]
	bufferCallbackAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
}

//export cronetBufferCallbackOnDestroy
func cronetBufferCallbackOnDestroy(self C.Cronet_BufferCallbackPtr, buffer C.Cronet_BufferPtr) {
	ptrInt := uintptr(unsafe.Pointer(self))
	bufferCallbackAccess.RLock()
	entry := bufferCallbackMap[ptrInt]
	bufferCallbackAccess.RUnlock()
	if entry == nil || entry.destroyed.Load() {
		return
	}
	if entry.callback != nil {
		entry.callback(BufferCallback{ptrInt}, Buffer{uintptr(unsafe.Pointer(buffer))})
	}
	bufferCallbackAccess.Lock()
	delete(bufferCallbackMap, ptrInt)
	bufferCallbackAccess.Unlock()
}
