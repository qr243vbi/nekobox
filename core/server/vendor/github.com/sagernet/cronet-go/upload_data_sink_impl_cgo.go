//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT void cronetUploadDataSinkOnReadSucceeded(Cronet_UploadDataSinkPtr self, uint64_t bytes_read, bool final_chunk);
// extern CRONET_EXPORT void cronetUploadDataSinkOnReadError(Cronet_UploadDataSinkPtr self, Cronet_String error_message);
// extern CRONET_EXPORT void cronetUploadDataSinkOnRewindSucceeded(Cronet_UploadDataSinkPtr self);
// extern CRONET_EXPORT void cronetUploadDataSinkOnRewindError(Cronet_UploadDataSinkPtr self, Cronet_String error_message);
import "C"

import (
	"sync"
	"unsafe"
)

// UploadDataSinkHandler is an interface for custom UploadDataSink implementations (for testing/mocking).
type UploadDataSinkHandler interface {
	OnReadSucceeded(self UploadDataSink, bytesRead uint64, finalChunk bool)
	OnReadError(self UploadDataSink, errorMessage string)
	OnRewindSucceeded(self UploadDataSink)
	OnRewindError(self UploadDataSink, errorMessage string)
}

// NewUploadDataSinkWith creates a new UploadDataSink with custom handler (for testing/mocking).
func NewUploadDataSinkWith(handler UploadDataSinkHandler) UploadDataSink {
	ptr := C.Cronet_UploadDataSink_CreateWith(
		(*[0]byte)(C.cronetUploadDataSinkOnReadSucceeded),
		(*[0]byte)(C.cronetUploadDataSinkOnReadError),
		(*[0]byte)(C.cronetUploadDataSinkOnRewindSucceeded),
		(*[0]byte)(C.cronetUploadDataSinkOnRewindError),
	)
	ptrVal := uintptr(unsafe.Pointer(ptr))
	uploadDataSinkHandlerAccess.Lock()
	uploadDataSinkHandlerMap[ptrVal] = handler
	uploadDataSinkHandlerAccess.Unlock()
	return UploadDataSink{ptrVal}
}

var (
	uploadDataSinkHandlerAccess sync.RWMutex
	uploadDataSinkHandlerMap    map[uintptr]UploadDataSinkHandler
)

func init() {
	uploadDataSinkHandlerMap = make(map[uintptr]UploadDataSinkHandler)
}

func instanceOfUploadDataSinkHandler(self C.Cronet_UploadDataSinkPtr) UploadDataSinkHandler {
	uploadDataSinkHandlerAccess.RLock()
	defer uploadDataSinkHandlerAccess.RUnlock()
	return uploadDataSinkHandlerMap[uintptr(unsafe.Pointer(self))]
}

//export cronetUploadDataSinkOnReadSucceeded
func cronetUploadDataSinkOnReadSucceeded(self C.Cronet_UploadDataSinkPtr, bytesRead C.uint64_t, finalChunk C.bool) {
	handler := instanceOfUploadDataSinkHandler(self)
	if handler != nil {
		handler.OnReadSucceeded(UploadDataSink{uintptr(unsafe.Pointer(self))}, uint64(bytesRead), bool(finalChunk))
	}
}

//export cronetUploadDataSinkOnReadError
func cronetUploadDataSinkOnReadError(self C.Cronet_UploadDataSinkPtr, errorMessage C.Cronet_String) {
	handler := instanceOfUploadDataSinkHandler(self)
	if handler != nil {
		handler.OnReadError(UploadDataSink{uintptr(unsafe.Pointer(self))}, C.GoString(errorMessage))
	}
}

//export cronetUploadDataSinkOnRewindSucceeded
func cronetUploadDataSinkOnRewindSucceeded(self C.Cronet_UploadDataSinkPtr) {
	handler := instanceOfUploadDataSinkHandler(self)
	if handler != nil {
		handler.OnRewindSucceeded(UploadDataSink{uintptr(unsafe.Pointer(self))})
	}
}

//export cronetUploadDataSinkOnRewindError
func cronetUploadDataSinkOnRewindError(self C.Cronet_UploadDataSinkPtr, errorMessage C.Cronet_String) {
	handler := instanceOfUploadDataSinkHandler(self)
	if handler != nil {
		handler.OnRewindError(UploadDataSink{uintptr(unsafe.Pointer(self))}, C.GoString(errorMessage))
	}
}
