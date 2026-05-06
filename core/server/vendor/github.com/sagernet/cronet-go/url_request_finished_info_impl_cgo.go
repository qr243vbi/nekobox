//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT void cronetURLRequestFinishedInfoListenerOnRequestFinished(Cronet_RequestFinishedInfoListenerPtr self, Cronet_RequestFinishedInfoPtr request_info, Cronet_UrlResponseInfoPtr response_info, Cronet_ErrorPtr error);
import "C"

import (
	"sync"
	"sync/atomic"
	"unsafe"
)

type urlRequestFinishedInfoListenerEntry struct {
	handler   URLRequestFinishedInfoListenerOnRequestFinishedFunc
	destroyed atomic.Bool
}

var (
	urlRequestFinishedInfoListenerAccess sync.RWMutex
	urlRequestFinishedInfoListenerMap    map[uintptr]*urlRequestFinishedInfoListenerEntry
)

func init() {
	urlRequestFinishedInfoListenerMap = make(map[uintptr]*urlRequestFinishedInfoListenerEntry)
}

func NewURLRequestFinishedInfoListener(finishedFunc URLRequestFinishedInfoListenerOnRequestFinishedFunc) URLRequestFinishedInfoListener {
	if finishedFunc == nil {
		panic("nil url request finished info listener function")
	}
	ptr := C.Cronet_RequestFinishedInfoListener_CreateWith((*[0]byte)(C.cronetURLRequestFinishedInfoListenerOnRequestFinished))
	ptrVal := uintptr(unsafe.Pointer(ptr))
	urlRequestFinishedInfoListenerAccess.Lock()
	urlRequestFinishedInfoListenerMap[ptrVal] = &urlRequestFinishedInfoListenerEntry{handler: finishedFunc}
	urlRequestFinishedInfoListenerAccess.Unlock()
	return URLRequestFinishedInfoListener{ptrVal}
}

func (l URLRequestFinishedInfoListener) Destroy() {
	urlRequestFinishedInfoListenerAccess.Lock()
	entry := urlRequestFinishedInfoListenerMap[l.ptr]
	if entry != nil {
		entry.destroyed.Store(true)
	}
	urlRequestFinishedInfoListenerAccess.Unlock()
	C.Cronet_RequestFinishedInfoListener_Destroy(C.Cronet_RequestFinishedInfoListenerPtr(unsafe.Pointer(l.ptr)))
	// Cleanup after C destroy
	urlRequestFinishedInfoListenerAccess.Lock()
	delete(urlRequestFinishedInfoListenerMap, l.ptr)
	urlRequestFinishedInfoListenerAccess.Unlock()
}

//export cronetURLRequestFinishedInfoListenerOnRequestFinished
func cronetURLRequestFinishedInfoListenerOnRequestFinished(self C.Cronet_RequestFinishedInfoListenerPtr, requestInfo C.Cronet_RequestFinishedInfoPtr, responseInfo C.Cronet_UrlResponseInfoPtr, error C.Cronet_ErrorPtr) {
	ptr := uintptr(unsafe.Pointer(self))
	urlRequestFinishedInfoListenerAccess.RLock()
	entry := urlRequestFinishedInfoListenerMap[ptr]
	urlRequestFinishedInfoListenerAccess.RUnlock()
	if entry == nil || entry.destroyed.Load() {
		return // Post-destroy callback, silently ignore
	}
	entry.handler(URLRequestFinishedInfoListener{ptr}, RequestFinishedInfo{uintptr(unsafe.Pointer(requestInfo))}, URLResponseInfo{uintptr(unsafe.Pointer(responseInfo))}, Error{uintptr(unsafe.Pointer(error))})
}
