//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT int64_t cronetUploadDataProviderGetLength(Cronet_UploadDataProviderPtr self);
// extern CRONET_EXPORT void cronetUploadDataProviderRead(Cronet_UploadDataProviderPtr self, Cronet_UploadDataSinkPtr upload_data_sink, Cronet_BufferPtr buffer);
// extern CRONET_EXPORT void cronetUploadDataProviderRewind(Cronet_UploadDataProviderPtr self, Cronet_UploadDataSinkPtr upload_data_sink);
// extern CRONET_EXPORT void cronetUploadDataProviderClose(Cronet_UploadDataProviderPtr self);
import "C"

import (
	"sync"
	"sync/atomic"
	"unsafe"
)

type uploadDataProviderEntry struct {
	handler   UploadDataProviderHandler
	destroyed atomic.Bool
}

var (
	uploadDataAccess      sync.RWMutex
	uploadDataProviderMap map[uintptr]*uploadDataProviderEntry
)

func init() {
	uploadDataProviderMap = make(map[uintptr]*uploadDataProviderEntry)
}

func NewUploadDataProvider(handler UploadDataProviderHandler) UploadDataProvider {
	if handler == nil {
		panic("nil upload data provider handler")
	}
	ptr := C.Cronet_UploadDataProvider_CreateWith(
		(*[0]byte)(C.cronetUploadDataProviderGetLength),
		(*[0]byte)(C.cronetUploadDataProviderRead),
		(*[0]byte)(C.cronetUploadDataProviderRewind),
		(*[0]byte)(C.cronetUploadDataProviderClose),
	)
	ptrVal := uintptr(unsafe.Pointer(ptr))
	uploadDataAccess.Lock()
	uploadDataProviderMap[ptrVal] = &uploadDataProviderEntry{handler: handler}
	uploadDataAccess.Unlock()
	return UploadDataProvider{ptrVal}
}

func (p UploadDataProvider) Destroy() {
	uploadDataAccess.RLock()
	entry := uploadDataProviderMap[p.ptr]
	uploadDataAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
	C.Cronet_UploadDataProvider_Destroy(C.Cronet_UploadDataProviderPtr(unsafe.Pointer(p.ptr)))
}

func instanceOfUploadDataProvider(self C.Cronet_UploadDataProviderPtr) UploadDataProviderHandler {
	uploadDataAccess.RLock()
	defer uploadDataAccess.RUnlock()
	entry := uploadDataProviderMap[uintptr(unsafe.Pointer(self))]
	if entry == nil || entry.destroyed.Load() {
		return nil
	}
	return entry.handler
}

//export cronetUploadDataProviderGetLength
func cronetUploadDataProviderGetLength(self C.Cronet_UploadDataProviderPtr) C.int64_t {
	handler := instanceOfUploadDataProvider(self)
	if handler == nil {
		return 0 // Post-destroy callback, return 0
	}
	return C.int64_t(handler.Length(UploadDataProvider{uintptr(unsafe.Pointer(self))}))
}

//export cronetUploadDataProviderRead
func cronetUploadDataProviderRead(self C.Cronet_UploadDataProviderPtr, sink C.Cronet_UploadDataSinkPtr, buffer C.Cronet_BufferPtr) {
	handler := instanceOfUploadDataProvider(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.Read(UploadDataProvider{uintptr(unsafe.Pointer(self))}, UploadDataSink{uintptr(unsafe.Pointer(sink))}, Buffer{uintptr(unsafe.Pointer(buffer))})
}

//export cronetUploadDataProviderRewind
func cronetUploadDataProviderRewind(self C.Cronet_UploadDataProviderPtr, sink C.Cronet_UploadDataSinkPtr) {
	handler := instanceOfUploadDataProvider(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.Rewind(UploadDataProvider{uintptr(unsafe.Pointer(self))}, UploadDataSink{uintptr(unsafe.Pointer(sink))})
}

//export cronetUploadDataProviderClose
func cronetUploadDataProviderClose(self C.Cronet_UploadDataProviderPtr) {
	ptr := uintptr(unsafe.Pointer(self))
	handler := instanceOfUploadDataProvider(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.Close(UploadDataProvider{ptr})
	// Close is terminal callback - safe to cleanup
	uploadDataAccess.Lock()
	delete(uploadDataProviderMap, ptr)
	uploadDataAccess.Unlock()
}
