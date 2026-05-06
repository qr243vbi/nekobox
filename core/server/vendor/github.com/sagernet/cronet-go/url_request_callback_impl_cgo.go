//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT void cronetURLRequestCallbackOnRedirectReceived(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info, Cronet_String new_location_url);
// extern CRONET_EXPORT void cronetURLRequestCallbackOnResponseStarted(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info);
// extern CRONET_EXPORT void cronetURLRequestCallbackOnReadCompleted(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info, Cronet_BufferPtr buffer, uint64_t bytes_read);
// extern CRONET_EXPORT void cronetURLRequestCallbackOnSucceeded(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info);
// extern CRONET_EXPORT void cronetURLRequestCallbackOnFailed(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info, Cronet_ErrorPtr error);
// extern CRONET_EXPORT void cronetURLRequestCallbackOnCanceled(Cronet_UrlRequestCallbackPtr self, Cronet_UrlRequestPtr request, Cronet_UrlResponseInfoPtr info);
import "C"

import (
	"sync"
	"sync/atomic"
	"unsafe"
)

type urlRequestCallbackEntry struct {
	handler   URLRequestCallbackHandler
	destroyed atomic.Bool
}

var (
	urlRequestCallbackAccess sync.RWMutex
	urlRequestCallbackMap    map[uintptr]*urlRequestCallbackEntry
)

func init() {
	urlRequestCallbackMap = make(map[uintptr]*urlRequestCallbackEntry)
}

func NewURLRequestCallback(handler URLRequestCallbackHandler) URLRequestCallback {
	if handler == nil {
		panic("nil url request callback handler")
	}
	ptr := C.Cronet_UrlRequestCallback_CreateWith(
		(*[0]byte)(C.cronetURLRequestCallbackOnRedirectReceived),
		(*[0]byte)(C.cronetURLRequestCallbackOnResponseStarted),
		(*[0]byte)(C.cronetURLRequestCallbackOnReadCompleted),
		(*[0]byte)(C.cronetURLRequestCallbackOnSucceeded),
		(*[0]byte)(C.cronetURLRequestCallbackOnFailed),
		(*[0]byte)(C.cronetURLRequestCallbackOnCanceled),
	)
	ptrVal := uintptr(unsafe.Pointer(ptr))
	urlRequestCallbackAccess.Lock()
	urlRequestCallbackMap[ptrVal] = &urlRequestCallbackEntry{handler: handler}
	urlRequestCallbackAccess.Unlock()
	return URLRequestCallback{ptrVal}
}

func (c URLRequestCallback) Destroy() {
	urlRequestCallbackAccess.RLock()
	entry := urlRequestCallbackMap[c.ptr]
	urlRequestCallbackAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
	C.Cronet_UrlRequestCallback_Destroy(C.Cronet_UrlRequestCallbackPtr(unsafe.Pointer(c.ptr)))
}

func instanceOfURLRequestCallback(self C.Cronet_UrlRequestCallbackPtr) URLRequestCallbackHandler {
	urlRequestCallbackAccess.RLock()
	defer urlRequestCallbackAccess.RUnlock()
	entry := urlRequestCallbackMap[uintptr(unsafe.Pointer(self))]
	if entry == nil || entry.destroyed.Load() {
		return nil
	}
	return entry.handler
}

//export cronetURLRequestCallbackOnRedirectReceived
func cronetURLRequestCallbackOnRedirectReceived(self C.Cronet_UrlRequestCallbackPtr, request C.Cronet_UrlRequestPtr, info C.Cronet_UrlResponseInfoPtr, newLocationUrl C.Cronet_String) {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.OnRedirectReceived(URLRequestCallback{uintptr(unsafe.Pointer(self))}, URLRequest{uintptr(unsafe.Pointer(request))}, URLResponseInfo{uintptr(unsafe.Pointer(info))}, C.GoString(newLocationUrl))
}

//export cronetURLRequestCallbackOnResponseStarted
func cronetURLRequestCallbackOnResponseStarted(self C.Cronet_UrlRequestCallbackPtr, request C.Cronet_UrlRequestPtr, info C.Cronet_UrlResponseInfoPtr) {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.OnResponseStarted(URLRequestCallback{uintptr(unsafe.Pointer(self))}, URLRequest{uintptr(unsafe.Pointer(request))}, URLResponseInfo{uintptr(unsafe.Pointer(info))})
}

//export cronetURLRequestCallbackOnReadCompleted
func cronetURLRequestCallbackOnReadCompleted(self C.Cronet_UrlRequestCallbackPtr, request C.Cronet_UrlRequestPtr, info C.Cronet_UrlResponseInfoPtr, buffer C.Cronet_BufferPtr, bytesRead C.uint64_t) {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.OnReadCompleted(URLRequestCallback{uintptr(unsafe.Pointer(self))}, URLRequest{uintptr(unsafe.Pointer(request))}, URLResponseInfo{uintptr(unsafe.Pointer(info))}, Buffer{uintptr(unsafe.Pointer(buffer))}, int64(bytesRead))
}

//export cronetURLRequestCallbackOnSucceeded
func cronetURLRequestCallbackOnSucceeded(self C.Cronet_UrlRequestCallbackPtr, request C.Cronet_UrlRequestPtr, info C.Cronet_UrlResponseInfoPtr) {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.OnSucceeded(URLRequestCallback{uintptr(unsafe.Pointer(self))}, URLRequest{uintptr(unsafe.Pointer(request))}, URLResponseInfo{uintptr(unsafe.Pointer(info))})
	// Terminal callback - safe to cleanup
	cleanupURLRequestCallback(uintptr(unsafe.Pointer(self)))
}

//export cronetURLRequestCallbackOnFailed
func cronetURLRequestCallbackOnFailed(self C.Cronet_UrlRequestCallbackPtr, request C.Cronet_UrlRequestPtr, info C.Cronet_UrlResponseInfoPtr, error C.Cronet_ErrorPtr) {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.OnFailed(URLRequestCallback{uintptr(unsafe.Pointer(self))}, URLRequest{uintptr(unsafe.Pointer(request))}, URLResponseInfo{uintptr(unsafe.Pointer(info))}, Error{uintptr(unsafe.Pointer(error))})
	// Terminal callback - safe to cleanup
	cleanupURLRequestCallback(uintptr(unsafe.Pointer(self)))
}

//export cronetURLRequestCallbackOnCanceled
func cronetURLRequestCallbackOnCanceled(self C.Cronet_UrlRequestCallbackPtr, request C.Cronet_UrlRequestPtr, info C.Cronet_UrlResponseInfoPtr) {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return // Post-destroy callback, silently ignore
	}
	handler.OnCanceled(URLRequestCallback{uintptr(unsafe.Pointer(self))}, URLRequest{uintptr(unsafe.Pointer(request))}, URLResponseInfo{uintptr(unsafe.Pointer(info))})
	// Terminal callback - safe to cleanup
	cleanupURLRequestCallback(uintptr(unsafe.Pointer(self)))
}

func cleanupURLRequestCallback(ptr uintptr) {
	urlRequestCallbackAccess.Lock()
	delete(urlRequestCallbackMap, ptr)
	urlRequestCallbackAccess.Unlock()
}
