//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
// extern CRONET_EXPORT Cronet_RESULT cronetUrlRequestInitWithParams(Cronet_UrlRequestPtr self, Cronet_EnginePtr engine, Cronet_String url, Cronet_UrlRequestParamsPtr params, Cronet_UrlRequestCallbackPtr callback, Cronet_ExecutorPtr executor);
// extern CRONET_EXPORT Cronet_RESULT cronetUrlRequestStart(Cronet_UrlRequestPtr self);
// extern CRONET_EXPORT Cronet_RESULT cronetUrlRequestFollowRedirect(Cronet_UrlRequestPtr self);
// extern CRONET_EXPORT Cronet_RESULT cronetUrlRequestRead(Cronet_UrlRequestPtr self, Cronet_BufferPtr buffer);
// extern CRONET_EXPORT void cronetUrlRequestCancel(Cronet_UrlRequestPtr self);
// extern CRONET_EXPORT bool cronetUrlRequestIsDone(Cronet_UrlRequestPtr self);
// extern CRONET_EXPORT void cronetUrlRequestGetStatus(Cronet_UrlRequestPtr self, Cronet_UrlRequestStatusListenerPtr listener);
import "C"

import (
	"sync"
	"unsafe"
)

// URLRequestHandler is an interface for custom URLRequest implementations (for testing/mocking).
type URLRequestHandler interface {
	InitWithParams(self URLRequest, engine Engine, url string, params URLRequestParams, callback URLRequestCallback, executor Executor) Result
	Start(self URLRequest) Result
	FollowRedirect(self URLRequest) Result
	Read(self URLRequest, buffer Buffer) Result
	Cancel(self URLRequest)
	IsDone(self URLRequest) bool
	GetStatus(self URLRequest, listener URLRequestStatusListener)
}

// NewURLRequestWith creates a new URLRequest with custom handler (for testing/mocking).
func NewURLRequestWith(handler URLRequestHandler) URLRequest {
	ptr := C.Cronet_UrlRequest_CreateWith(
		(*[0]byte)(C.cronetUrlRequestInitWithParams),
		(*[0]byte)(C.cronetUrlRequestStart),
		(*[0]byte)(C.cronetUrlRequestFollowRedirect),
		(*[0]byte)(C.cronetUrlRequestRead),
		(*[0]byte)(C.cronetUrlRequestCancel),
		(*[0]byte)(C.cronetUrlRequestIsDone),
		(*[0]byte)(C.cronetUrlRequestGetStatus),
	)
	ptrVal := uintptr(unsafe.Pointer(ptr))
	urlRequestHandlerAccess.Lock()
	urlRequestHandlerMap[ptrVal] = handler
	urlRequestHandlerAccess.Unlock()
	return URLRequest{ptrVal}
}

var (
	urlRequestHandlerAccess sync.RWMutex
	urlRequestHandlerMap    map[uintptr]URLRequestHandler
)

func init() {
	urlRequestHandlerMap = make(map[uintptr]URLRequestHandler)
}

func instanceOfURLRequestHandler(self C.Cronet_UrlRequestPtr) URLRequestHandler {
	urlRequestHandlerAccess.RLock()
	defer urlRequestHandlerAccess.RUnlock()
	return urlRequestHandlerMap[uintptr(unsafe.Pointer(self))]
}

//export cronetUrlRequestInitWithParams
func cronetUrlRequestInitWithParams(self C.Cronet_UrlRequestPtr, engine C.Cronet_EnginePtr, url C.Cronet_String, params C.Cronet_UrlRequestParamsPtr, callback C.Cronet_UrlRequestCallbackPtr, executor C.Cronet_ExecutorPtr) C.Cronet_RESULT {
	handler := instanceOfURLRequestHandler(self)
	if handler != nil {
		return C.Cronet_RESULT(handler.InitWithParams(URLRequest{uintptr(unsafe.Pointer(self))}, Engine{uintptr(unsafe.Pointer(engine))}, C.GoString(url), URLRequestParams{uintptr(unsafe.Pointer(params))}, URLRequestCallback{uintptr(unsafe.Pointer(callback))}, Executor{uintptr(unsafe.Pointer(executor))}))
	}
	return C.Cronet_RESULT_SUCCESS
}

//export cronetUrlRequestStart
func cronetUrlRequestStart(self C.Cronet_UrlRequestPtr) C.Cronet_RESULT {
	handler := instanceOfURLRequestHandler(self)
	if handler != nil {
		return C.Cronet_RESULT(handler.Start(URLRequest{uintptr(unsafe.Pointer(self))}))
	}
	return C.Cronet_RESULT_SUCCESS
}

//export cronetUrlRequestFollowRedirect
func cronetUrlRequestFollowRedirect(self C.Cronet_UrlRequestPtr) C.Cronet_RESULT {
	handler := instanceOfURLRequestHandler(self)
	if handler != nil {
		return C.Cronet_RESULT(handler.FollowRedirect(URLRequest{uintptr(unsafe.Pointer(self))}))
	}
	return C.Cronet_RESULT_SUCCESS
}

//export cronetUrlRequestRead
func cronetUrlRequestRead(self C.Cronet_UrlRequestPtr, buffer C.Cronet_BufferPtr) C.Cronet_RESULT {
	handler := instanceOfURLRequestHandler(self)
	if handler != nil {
		return C.Cronet_RESULT(handler.Read(URLRequest{uintptr(unsafe.Pointer(self))}, Buffer{uintptr(unsafe.Pointer(buffer))}))
	}
	return C.Cronet_RESULT_SUCCESS
}

//export cronetUrlRequestCancel
func cronetUrlRequestCancel(self C.Cronet_UrlRequestPtr) {
	handler := instanceOfURLRequestHandler(self)
	if handler != nil {
		handler.Cancel(URLRequest{uintptr(unsafe.Pointer(self))})
	}
}

//export cronetUrlRequestIsDone
func cronetUrlRequestIsDone(self C.Cronet_UrlRequestPtr) C.bool {
	handler := instanceOfURLRequestHandler(self)
	if handler != nil {
		return C.bool(handler.IsDone(URLRequest{uintptr(unsafe.Pointer(self))}))
	}
	return C.bool(false)
}

//export cronetUrlRequestGetStatus
func cronetUrlRequestGetStatus(self C.Cronet_UrlRequestPtr, listener C.Cronet_UrlRequestStatusListenerPtr) {
	handler := instanceOfURLRequestHandler(self)
	if handler != nil {
		handler.GetStatus(URLRequest{uintptr(unsafe.Pointer(self))}, URLRequestStatusListener{uintptr(unsafe.Pointer(listener))})
	}
}
