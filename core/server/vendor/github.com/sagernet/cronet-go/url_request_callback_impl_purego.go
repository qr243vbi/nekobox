//go:build with_purego && !386 && !arm && !mipsle

package cronet

import (
	"sync"
	"sync/atomic"

	"github.com/sagernet/cronet-go/internal/cronet"

	"github.com/ebitengine/purego"
)

type urlRequestCallbackEntry struct {
	handler   URLRequestCallbackHandler
	destroyed atomic.Bool
}

var (
	urlRequestCallbackAccess sync.RWMutex
	urlRequestCallbackMap    map[uintptr]*urlRequestCallbackEntry

	urlRequestCallbackOnRedirectReceived uintptr
	urlRequestCallbackOnResponseStarted  uintptr
	urlRequestCallbackOnReadCompleted    uintptr
	urlRequestCallbackOnSucceeded        uintptr
	urlRequestCallbackOnFailed           uintptr
	urlRequestCallbackOnCanceled         uintptr
)

func init() {
	urlRequestCallbackMap = make(map[uintptr]*urlRequestCallbackEntry)

	urlRequestCallbackOnRedirectReceived = purego.NewCallback(onRedirectReceivedCallback)
	urlRequestCallbackOnResponseStarted = purego.NewCallback(onResponseStartedCallback)
	urlRequestCallbackOnReadCompleted = purego.NewCallback(onReadCompletedCallback)
	urlRequestCallbackOnSucceeded = purego.NewCallback(onSucceededCallback)
	urlRequestCallbackOnFailed = purego.NewCallback(onFailedCallback)
	urlRequestCallbackOnCanceled = purego.NewCallback(onCanceledCallback)
}

func instanceOfURLRequestCallback(self uintptr) URLRequestCallbackHandler {
	urlRequestCallbackAccess.RLock()
	defer urlRequestCallbackAccess.RUnlock()
	entry := urlRequestCallbackMap[self]
	if entry == nil || entry.destroyed.Load() {
		return nil
	}
	return entry.handler
}

func onRedirectReceivedCallback(self, request, info, newLocationUrl uintptr) uintptr {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return 0 // Post-destroy callback, silently ignore
	}
	handler.OnRedirectReceived(
		URLRequestCallback{self},
		URLRequest{request},
		URLResponseInfo{info},
		cronet.GoString(newLocationUrl),
	)
	return 0
}

func onResponseStartedCallback(self, request, info uintptr) uintptr {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return 0 // Post-destroy callback, silently ignore
	}
	handler.OnResponseStarted(
		URLRequestCallback{self},
		URLRequest{request},
		URLResponseInfo{info},
	)
	return 0
}

func onReadCompletedCallback(self, request, info, buffer uintptr, bytesRead uint64) uintptr {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return 0 // Post-destroy callback, silently ignore
	}
	handler.OnReadCompleted(
		URLRequestCallback{self},
		URLRequest{request},
		URLResponseInfo{info},
		Buffer{buffer},
		int64(bytesRead),
	)
	return 0
}

func onSucceededCallback(self, request, info uintptr) uintptr {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return 0 // Post-destroy callback, silently ignore
	}
	handler.OnSucceeded(
		URLRequestCallback{self},
		URLRequest{request},
		URLResponseInfo{info},
	)
	// Terminal callback - safe to cleanup
	cleanupURLRequestCallback(self)
	return 0
}

func onFailedCallback(self, request, info, err uintptr) uintptr {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return 0 // Post-destroy callback, silently ignore
	}
	handler.OnFailed(
		URLRequestCallback{self},
		URLRequest{request},
		URLResponseInfo{info},
		Error{err},
	)
	// Terminal callback - safe to cleanup
	cleanupURLRequestCallback(self)
	return 0
}

func onCanceledCallback(self, request, info uintptr) uintptr {
	handler := instanceOfURLRequestCallback(self)
	if handler == nil {
		return 0 // Post-destroy callback, silently ignore
	}
	handler.OnCanceled(
		URLRequestCallback{self},
		URLRequest{request},
		URLResponseInfo{info},
	)
	// Terminal callback - safe to cleanup
	cleanupURLRequestCallback(self)
	return 0
}

func cleanupURLRequestCallback(ptr uintptr) {
	urlRequestCallbackAccess.Lock()
	delete(urlRequestCallbackMap, ptr)
	urlRequestCallbackAccess.Unlock()
}

func NewURLRequestCallback(handler URLRequestCallbackHandler) URLRequestCallback {
	if handler == nil {
		panic("nil url request callback handler")
	}
	ptr := cronet.UrlRequestCallbackCreateWith(
		urlRequestCallbackOnRedirectReceived,
		urlRequestCallbackOnResponseStarted,
		urlRequestCallbackOnReadCompleted,
		urlRequestCallbackOnSucceeded,
		urlRequestCallbackOnFailed,
		urlRequestCallbackOnCanceled,
	)
	urlRequestCallbackAccess.Lock()
	urlRequestCallbackMap[ptr] = &urlRequestCallbackEntry{handler: handler}
	urlRequestCallbackAccess.Unlock()
	return URLRequestCallback{ptr}
}

func (c URLRequestCallback) Destroy() {
	urlRequestCallbackAccess.RLock()
	entry := urlRequestCallbackMap[c.ptr]
	urlRequestCallbackAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
	cronet.UrlRequestCallbackDestroy(c.ptr)
}
