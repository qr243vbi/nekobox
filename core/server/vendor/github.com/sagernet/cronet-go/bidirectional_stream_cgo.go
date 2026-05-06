//go:build !with_purego

package cronet

// #include <stdbool.h>
// #include <stdlib.h>
// #include <cronet_c.h>
// #include <bidirectional_stream_c.h>
import "C"

import (
	"unsafe"
)

func (e Engine) StreamEngine() StreamEngine {
	return StreamEngine{uintptr(unsafe.Pointer(C.Cronet_Engine_GetStreamEngine(C.Cronet_EnginePtr(unsafe.Pointer(e.ptr)))))}
}

// CreateStream
// Creates a new stream object that uses |engine| and |callback|. All stream
// tasks are performed asynchronously on the |engine| network thread. |callback|
// methods are invoked synchronously on the |engine| network thread, but must
// not run tasks on the current thread to prevent blocking networking operations
// and causing exceptions during shutdown. The |annotation| is stored in
// bidirectional stream for arbitrary use by application.
//
// Returned |bidirectional_stream*| is owned by the caller, and must be
// destroyed using |bidirectional_stream_destroy|.
//
// Both |calback| and |engine| must remain valid until stream is destroyed.
func (e StreamEngine) CreateStream(callback BidirectionalStreamCallback) BidirectionalStream {
	if callback == nil {
		panic("nil bidirectional stream callback")
	}
	ptr := C.bidirectional_stream_create((*C.stream_engine)(unsafe.Pointer(e.ptr)), nil, &bidirectionalStreamCallbackCGO)
	ptrVal := uintptr(unsafe.Pointer(ptr))
	bidirectionalStreamAccess.Lock()
	bidirectionalStreamMap[ptrVal] = &bidirectionalStreamEntry{callback: callback}
	bidirectionalStreamAccess.Unlock()
	return BidirectionalStream{ptrVal}
}

// Destroy destroys stream object. Destroy could be called from any thread, including
// network thread, but is posted, so |stream| is valid until calling task is
// complete. The destroy operation is asynchronous - callbacks may still be
// invoked after this returns. The stream is marked as destroyed and callbacks
// will silently return.
func (c BidirectionalStream) Destroy() bool {
	bidirectionalStreamAccess.RLock()
	entry := bidirectionalStreamMap[c.ptr]
	bidirectionalStreamAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
	return C.bidirectional_stream_destroy((*C.bidirectional_stream)(unsafe.Pointer(c.ptr))) == 0
}

// DisableAutoFlush disables or enables auto flush. By default, data is flushed after
// every Write(). If the auto flush is disabled,
// the client should explicitly call Flush() to flush
// the data.
func (c BidirectionalStream) DisableAutoFlush(disable bool) {
	C.bidirectional_stream_disable_auto_flush((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)), C.bool(disable))
}

// DelayRequestHeadersUntilFlush delays sending request headers until Flush()
// is called. This flag is currently only respected when QUIC is negotiated.
// When true, QUIC will send request header frame along with data frame(s)
// as a single packet when possible.
func (c BidirectionalStream) DelayRequestHeadersUntilFlush(delay bool) {
	C.bidirectional_stream_delay_request_headers_until_flush((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)), C.bool(delay))
}

// Start starts the stream by sending request to |url| using |method| and |headers|.
// If |endOfStream| is true, then no data is expected to be written. The
// |method| is HTTP verb.
// noinspection GoDeferInLoop
func (c BidirectionalStream) Start(method string, url string, headers map[string]string, priority int, endOfStream bool) bool {
	var headerArray C.bidirectional_stream_header_array
	headerLen := len(headers)
	if headerLen > 0 {
		cHeadersPtr := C.malloc(C.size_t(int(C.sizeof_struct_bidirectional_stream_header) * headerLen))
		defer C.free(cHeadersPtr)
		cType := (*C.bidirectional_stream_header)(cHeadersPtr)
		cHeaders := unsafe.Slice(cType, headerLen)
		var index int
		for key, value := range headers {
			cKey := C.CString(key)
			defer C.free(unsafe.Pointer(cKey))
			cValue := C.CString(value)
			defer C.free(unsafe.Pointer(cValue))
			cHeaders[index].key = cKey
			cHeaders[index].value = cValue
			index++
		}
		headerArray = C.bidirectional_stream_header_array{
			C.size_t(headerLen), C.size_t(headerLen), &cHeaders[0],
		}
	}

	cMethod := C.CString(method)
	defer C.free(unsafe.Pointer(cMethod))

	cURL := C.CString(url)
	defer C.free(unsafe.Pointer(cURL))

	return C.bidirectional_stream_start((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)), cURL, C.int(priority), cMethod, &headerArray, C.bool(endOfStream)) == 0
}

// Read reads response data into |buffer|. Must only be called
// at most once in response to each invocation of the
// OnStreamReady()/OnResponseHeaderReceived() and OnReadCompleted()
// methods of the BidirectionalStreamCallback.
// Each call will result in an invocation of the callback's
// OnReadCompleted() method if data is read, or its OnFailed() method if
// there's an error. The callback's OnSucceeded() method is also invoked if
// there is no more data to read and |end_of_stream| was previously sent.
func (c BidirectionalStream) Read(buffer []byte) int {
	if len(buffer) == 0 {
		return int(C.bidirectional_stream_read((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)), nil, 0))
	}
	return int(C.bidirectional_stream_read((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)), (*C.char)((unsafe.Pointer)(&buffer[0])), C.int(len(buffer))))
}

// Write Writes request data from |buffer| If auto flush is
// disabled, data will be sent only after Flush() is
// called.
// Each call will result in an invocation the callback's BidirectionalStreamCallback.OnWriteCompleted()
// method if data is sent, or its BidirectionalStreamCallback.OnFailed() method if there's an error.
// The callback's BidirectionalStreamCallback.OnSucceeded() method is also invoked if |endOfStream| is
// set and all response data has been read.
func (c BidirectionalStream) Write(buffer []byte, endOfStream bool) int {
	if len(buffer) == 0 {
		return int(C.bidirectional_stream_write((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)), nil, 0, C.bool(endOfStream)))
	}
	return int(C.bidirectional_stream_write((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)), (*C.char)(unsafe.Pointer(&buffer[0])), C.int(len(buffer)), C.bool(endOfStream)))
}

// Flush Flushes pending writes. This method should not be called before invocation of
// BidirectionalStreamCallback.OnStreamReady() method.
// For each previously called Write()
// a corresponding OnWriteCompleted() callback will be invoked when the buffer
// is sent.BidirectionalStream
func (c BidirectionalStream) Flush() {
	C.bidirectional_stream_flush((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)))
}

// Cancel cancels the stream. Can be called at any time after
// Start(). The BidirectionalStreamCallback.OnCanceled() method will be invoked when cancellation
// is complete and no further callback methods will be invoked. If the
// stream has completed or has not started, calling
// Cancel() has no effect and BidirectionalStreamCallback.OnCanceled() will not
// be invoked. At most one callback method may be invoked after
// Cancel() has completed.
func (c BidirectionalStream) Cancel() {
	C.bidirectional_stream_cancel((*C.bidirectional_stream)(unsafe.Pointer(c.ptr)))
}
