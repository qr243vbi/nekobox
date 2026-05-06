//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"
import "unsafe"

func NewURLRequest() URLRequest {
	return URLRequest{uintptr(unsafe.Pointer(C.Cronet_UrlRequest_Create()))}
}

func (r URLRequest) Destroy() {
	C.Cronet_UrlRequest_Destroy(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr)))
}

// InitWithParams
// Initialized URLRequest to |url| with |params|. All methods of |callback| for
// request will be invoked on |executor|. The |executor| must not run tasks on
// the thread calling Executor.Execute() to prevent blocking networking
// operations and causing failure RESULTs during shutdown.
//
// @param engine Engine to process the request.
// @param url URL for the request.
// @param params additional parameters for the request, like headers and priority.
// @param callback Callback that gets invoked on different events.
// @param executor Executor on which all callbacks will be invoked.
func (r URLRequest) InitWithParams(engine Engine, url string, params URLRequestParams, callback URLRequestCallback, executor Executor) Result {
	cURL := C.CString(url)
	defer C.free(unsafe.Pointer(cURL))

	return Result(C.Cronet_UrlRequest_InitWithParams(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr)), C.Cronet_EnginePtr(unsafe.Pointer(engine.ptr)), cURL, C.Cronet_UrlRequestParamsPtr(unsafe.Pointer(params.ptr)), C.Cronet_UrlRequestCallbackPtr(unsafe.Pointer(callback.ptr)), C.Cronet_ExecutorPtr(unsafe.Pointer(executor.ptr))))
}

// Start starts the request, all callbacks go to URLRequestCallbackHandler. May only be called
// once. May not be called if Cancel() has been called.
func (r URLRequest) Start() Result {
	return Result(C.Cronet_UrlRequest_Start(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr))))
}

// FollowRedirect
// Follows a pending redirect. Must only be called at most once for each
// invocation of URLRequestCallbackHandler.OnRedirectReceived().
func (r URLRequest) FollowRedirect() Result {
	return Result(C.Cronet_UrlRequest_FollowRedirect(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr))))
}

// Read
// Attempts to read part of the response body into the provided buffer.
// Must only be called at most once in response to each invocation of the
// URLRequestCallbackHandler.OnResponseStarted() and
// URLRequestCallbackHandler.OnReadCompleted()} methods of the URLRequestCallbackHandler.
// Each call will result in an asynchronous call to
// either the URLRequestCallbackHandler.OnReadCompleted() method if data
// is read, its URLRequestCallbackHandler.OnSucceeded() method if
// there's no more data to read, or its URLRequestCallbackHandler.OnFailed()
// method if there's an error.
// This method transfers ownership of |buffer| to Cronet, and app should
// not access it until one of these callbacks is invoked.
//
// @param buffer to write response body to. The app must not read or
//
//	modify buffer's position, limit, or data between its position and
//	limit until the request calls back into the URLRequestCallbackHandler.
func (r URLRequest) Read(buffer Buffer) Result {
	return Result(C.Cronet_UrlRequest_Read(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr)), C.Cronet_BufferPtr(unsafe.Pointer(buffer.ptr))))
}

// Cancel
// cancels the request. Can be called at any time.
// URLRequestCallbackHandler.OnCanceled() will be invoked when cancellation
// is complete and no further callback methods will be invoked. If the
// request has completed or has not started, calling Cancel() has no
// effect and URLRequestCallbackHandler.OnCanceled() will not be invoked. If the
// Executor passed in to UrlRequest.InitWithParams() runs
// tasks on a single thread, and Cancel() is called on that thread,
// no callback methods (besides URLRequestCallbackHandler.OnCanceled() will be invoked after
// Cancel() is called. Otherwise, at most one callback method may be
// invoked after Cancel() has completed.
func (r URLRequest) Cancel() {
	C.Cronet_UrlRequest_Cancel(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr)))
}

// IsDone
// Returns true if the request was successfully started and is now
// finished (completed, canceled, or failed).
func (r URLRequest) IsDone() bool {
	return bool(C.Cronet_UrlRequest_IsDone(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr))))
}

// GetStatus
// Queries the status of the request.
// @param listener a URLRequestStatusListener that will be invoked with
//
//	the request's current status. Listener will be invoked
//	back on the Executor passed in when the request was
//	created.
func (r URLRequest) GetStatus(listener URLRequestStatusListener) {
	C.Cronet_UrlRequest_GetStatus(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr)), C.Cronet_UrlRequestStatusListenerPtr(unsafe.Pointer(listener.ptr)))
}

func (r URLRequest) SetClientContext(context unsafe.Pointer) {
	C.Cronet_UrlRequest_SetClientContext(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr)), C.Cronet_ClientContext(context))
}

func (r URLRequest) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_UrlRequest_GetClientContext(C.Cronet_UrlRequestPtr(unsafe.Pointer(r.ptr))))
}
