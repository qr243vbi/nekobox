//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT void cronetURLRequestStatusListenerOnStatus(Cronet_UrlRequestStatusListenerPtr self, Cronet_UrlRequestStatusListener_Status status);
import "C"

import (
	"sync"
	"unsafe"
)

func NewURLRequestStatusListener(onStatusFunc URLRequestStatusListenerOnStatusFunc) URLRequestStatusListener {
	if onStatusFunc == nil {
		panic("nil url request status listener function")
	}
	ptr := C.Cronet_UrlRequestStatusListener_CreateWith((*[0]byte)(C.cronetURLRequestStatusListenerOnStatus))
	ptrVal := uintptr(unsafe.Pointer(ptr))
	urlRequestStatusListenerAccess.Lock()
	urlRequestStatusListenerMap[ptrVal] = onStatusFunc
	urlRequestStatusListenerAccess.Unlock()
	return URLRequestStatusListener{ptrVal}
}

func (l URLRequestStatusListener) Destroy() {
	C.Cronet_UrlRequestStatusListener_Destroy(C.Cronet_UrlRequestStatusListenerPtr(unsafe.Pointer(l.ptr)))
	urlRequestStatusListenerAccess.Lock()
	delete(urlRequestStatusListenerMap, l.ptr)
	urlRequestStatusListenerAccess.Unlock()
}

var (
	urlRequestStatusListenerAccess sync.RWMutex
	urlRequestStatusListenerMap    map[uintptr]URLRequestStatusListenerOnStatusFunc
)

func init() {
	urlRequestStatusListenerMap = make(map[uintptr]URLRequestStatusListenerOnStatusFunc)
}

//export cronetURLRequestStatusListenerOnStatus
func cronetURLRequestStatusListenerOnStatus(self C.Cronet_UrlRequestStatusListenerPtr, status C.Cronet_UrlRequestStatusListener_Status) {
	ptr := uintptr(unsafe.Pointer(self))
	urlRequestStatusListenerAccess.Lock()
	listener := urlRequestStatusListenerMap[ptr]
	delete(urlRequestStatusListenerMap, ptr)
	urlRequestStatusListenerAccess.Unlock()
	if listener == nil {
		return // Race with Destroy() or already invoked - silently return
	}
	listener(URLRequestStatusListener{ptr}, URLRequestStatusListenerStatus(status))
}
