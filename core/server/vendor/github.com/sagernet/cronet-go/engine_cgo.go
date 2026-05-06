//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <string.h>
// #include <cronet_c.h>
//
// extern CRONET_EXPORT int cronetDialerCallback(void* context, char* address, uint16_t port);
// extern CRONET_EXPORT int cronetUdpDialerCallback(void* context, char* address, uint16_t port, char* out_local_address, uint16_t* out_local_port);
import "C"

import (
	"sync"
	"unsafe"
)

var (
	dialerAccess    sync.RWMutex
	dialerMap       = make(map[uintptr]Dialer)
	udpDialerAccess sync.RWMutex
	udpDialerMap    = make(map[uintptr]UDPDialer)
)

//export cronetDialerCallback
func cronetDialerCallback(context unsafe.Pointer, address *C.char, port C.uint16_t) C.int {
	dialerAccess.RLock()
	dialer, ok := dialerMap[uintptr(context)]
	dialerAccess.RUnlock()
	if !ok {
		return -104 // ERR_CONNECTION_FAILED
	}
	return C.int(dialer(C.GoString(address), uint16(port)))
}

//export cronetUdpDialerCallback
func cronetUdpDialerCallback(context unsafe.Pointer, address *C.char, port C.uint16_t, outLocalAddress *C.char, outLocalPort *C.uint16_t) C.int {
	udpDialerAccess.RLock()
	dialer, ok := udpDialerMap[uintptr(context)]
	udpDialerAccess.RUnlock()
	if !ok {
		return -104 // ERR_CONNECTION_FAILED
	}
	fd, localAddress, localPort := dialer(C.GoString(address), uint16(port))

	// Write output parameters
	if outLocalAddress != nil && localAddress != "" {
		localAddressC := C.CString(localAddress)
		C.strcpy(outLocalAddress, localAddressC)
		C.free(unsafe.Pointer(localAddressC))
	}
	if outLocalPort != nil {
		*outLocalPort = C.uint16_t(localPort)
	}

	return C.int(fd)
}

func NewEngine() Engine {
	return Engine{uintptr(unsafe.Pointer(C.Cronet_Engine_Create()))}
}

func (e Engine) Destroy() {
	dialerAccess.Lock()
	delete(dialerMap, e.ptr)
	dialerAccess.Unlock()
	udpDialerAccess.Lock()
	delete(udpDialerMap, e.ptr)
	udpDialerAccess.Unlock()
	C.Cronet_Engine_Destroy(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)))
}

// StartWithParams starts Engine using given |params|. The engine must be started once
// and only once before other methods can be used.
func (e Engine) StartWithParams(params EngineParams) Result {
	return Result(C.Cronet_Engine_StartWithParams(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)), C.Cronet_EngineParamsPtr(unsafe.Pointer(params.ptr))))
}

// StartNetLogToFile starts NetLog logging to a file. The NetLog will contain events emitted
// by all live Engines. The NetLog is useful for debugging.
// The file can be viewed using a Chrome browser navigated to
// chrome://net-internals/#import
// Returns |true| if netlog has started successfully, |false| otherwise.
// @param fileName the complete file path. It must not be empty. If the file
//
//	exists, it is truncated before starting. If actively logging,
//	this method is ignored.
//
// @param logAll to include basic events, user cookies,
//
//	credentials and all transferred bytes in the log. This option presents
//	a privacy risk, since it exposes the user's credentials, and should
//	only be used with the user's consent and in situations where the log
//	won't be public. false to just include basic events.
func (e Engine) StartNetLogToFile(fileName string, logAll bool) bool {
	cPath := C.CString(fileName)
	result := C.Cronet_Engine_StartNetLogToFile(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)), cPath, C.bool(logAll))
	C.free(unsafe.Pointer(cPath))
	return bool(result)
}

// StopNetLog Stops NetLog logging and flushes file to disk. If a logging session is
// not in progress, this call is ignored. This method blocks until the log is
// closed to ensure that log file is complete and available.
func (e Engine) StopNetLog() {
	C.Cronet_Engine_StopNetLog(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)))
}

// Shutdown shuts down the Engine if there are no active requests,
// otherwise returns a failure Result.
//
// Cannot be called on network thread - the thread Cronet calls into
// Executor on (which is different from the thread the Executor invokes
// callbacks on). This method blocks until all the Engine's resources have
// been cleaned up.
func (e Engine) Shutdown() Result {
	return Result(C.Cronet_Engine_Shutdown(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr))))
}

// Version returns a human-readable version string of the engine.
func (e Engine) Version() string {
	return C.GoString(C.Cronet_Engine_GetVersionString(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr))))
}

// DefaultUserAgent Returns default human-readable version string of the engine. Can be used
// before StartWithParams() is called.
func (e Engine) DefaultUserAgent() string {
	return C.GoString(C.Cronet_Engine_GetDefaultUserAgent(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr))))
}

// AddRequestFinishListener registers a listener that gets called at the end of each request.
//
// The listener is called on Executor.
//
// The listener is called before URLRequestCallbackHandler.OnCanceled(),
// URLRequestCallbackHandler.OnFailed() or
// URLRequestCallbackHandler.OnSucceeded() is called -- note that if Executor
// runs the listener asynchronously, the actual call to the listener
// may happen after a URLRequestCallbackHandler method is called.
//
// Listeners are only guaranteed to be called for requests that are started
// after the listener is added.
//
// Ownership is **not** taken for listener or Executor.
//
// Assuming the listener won't run again (there are no pending requests with
// the listener attached, either via Engine or UrlRequest),
// the app may destroy it once its OnRequestFinished() has started,
// even inside that method.
//
// Similarly, the app may destroy executor in or after OnRequestFinished()}.
//
// It's also OK to destroy executor in or after one of
// URLRequestCallbackHandler.OnCanceled(), URLRequestCallbackHandler.OnFailed() or
// URLRequestCallbackHandler.OnSucceeded().
//
// Of course, both of these are only true if listener won't run again
// and executor isn't being used for anything else that might start
// running in the future.
//
// @param listener the listener for finished requests.
// @param executor the executor upon which to run listener.
func (e Engine) AddRequestFinishListener(listener URLRequestFinishedInfoListener, executor Executor) {
	C.Cronet_Engine_AddRequestFinishedListener(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)), C.Cronet_RequestFinishedInfoListenerPtr(unsafe.Pointer(listener.ptr)), C.Cronet_ExecutorPtr(unsafe.Pointer(executor.ptr)))
}

// RemoveRequestFinishListener unregisters a RequestFinishedInfoListener,
// including its association with its registered Executor.
func (e Engine) RemoveRequestFinishListener(listener URLRequestFinishedInfoListener) {
	C.Cronet_Engine_RemoveRequestFinishedListener(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)), C.Cronet_RequestFinishedInfoListenerPtr(unsafe.Pointer(listener.ptr)))
}

func (e Engine) SetClientContext(context unsafe.Pointer) {
	C.Cronet_Engine_SetClientContext(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)), C.Cronet_ClientContext(context))
}

func (e Engine) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_Engine_GetClientContext(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr))))
}

// SetTrustedRootCertificates sets custom trusted root certificates for this engine.
// Must be called before StartWithParams().
// pemRootCerts should be PEM-formatted certificates (can contain multiple certificates).
// Returns true if the certificates were successfully set, false if parsing failed.
func (e Engine) SetTrustedRootCertificates(pemRootCerts string) bool {
	cPem := C.CString(pemRootCerts)
	defer C.free(unsafe.Pointer(cPem))
	certVerifier := C.Cronet_CreateCertVerifierWithRootCerts(cPem)
	if certVerifier == nil {
		return false
	}
	C.Cronet_Engine_SetMockCertVerifierForTesting(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)), certVerifier)
	return true
}

// CloseAllConnections closes all connections managed by the engine's network session.
// This includes socket pools, HTTP stream pool, SPDY session pool, and QUIC session pool.
// Useful for releasing connection-related memory or speeding up engine shutdown.
func (e Engine) CloseAllConnections() {
	C.Cronet_Engine_CloseAllConnections(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)))
}

// SetDialer sets a custom dialer for TCP connections.
// When set, the engine will use this callback to establish TCP connections
// instead of the default system socket API.
// Must be called before StartWithParams().
// Pass nil to disable custom dialing.
func (e Engine) SetDialer(dialer Dialer) {
	if dialer == nil {
		C.Cronet_Engine_SetDialer(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)), nil, nil)
		dialerAccess.Lock()
		delete(dialerMap, e.ptr)
		dialerAccess.Unlock()
		return
	}
	dialerAccess.Lock()
	dialerMap[e.ptr] = dialer
	dialerAccess.Unlock()
	C.Cronet_Engine_SetDialer(
		C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)),
		(*[0]byte)(C.cronetDialerCallback),
		unsafe.Pointer(e.ptr),
	)
}

// SetUDPDialer sets a custom dialer for UDP sockets.
// When set, the engine will use this callback to create UDP sockets instead of
// the default system socket API.
// Must be called before StartWithParams().
// Pass nil to disable custom dialing.
func (e Engine) SetUDPDialer(dialer UDPDialer) {
	if dialer == nil {
		C.Cronet_Engine_SetUdpDialer(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)), nil, nil)
		udpDialerAccess.Lock()
		delete(udpDialerMap, e.ptr)
		udpDialerAccess.Unlock()
		return
	}
	udpDialerAccess.Lock()
	udpDialerMap[e.ptr] = dialer
	udpDialerAccess.Unlock()
	C.Cronet_Engine_SetUdpDialer(
		C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)),
		(*[0]byte)(C.cronetUdpDialerCallback),
		unsafe.Pointer(e.ptr),
	)
}
