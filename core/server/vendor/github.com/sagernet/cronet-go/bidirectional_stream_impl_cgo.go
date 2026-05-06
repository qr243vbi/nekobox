//go:build !with_purego

package cronet

// #include <stdbool.h>
// #include <stdlib.h>
// #include <cronet_c.h>
// #include <bidirectional_stream_c.h>
// extern CRONET_EXPORT void cronetBidirectionalStreamOnStreamReady(bidirectional_stream* stream);
// extern CRONET_EXPORT void cronetBidirectionalStreamOnResponseHeadersReceived(bidirectional_stream* stream, bidirectional_stream_header_array* headers, char* negotiated_protocol);
// extern CRONET_EXPORT void cronetBidirectionalStreamOnReadCompleted(bidirectional_stream* stream, char* data, int bytes_read);
// extern CRONET_EXPORT void cronetBidirectionalStreamOnWriteCompleted(bidirectional_stream* stream, char* data);
// extern CRONET_EXPORT void cronetBidirectionalStreamOnResponseTrailersReceived(bidirectional_stream* stream, bidirectional_stream_header_array* trailers);
// extern CRONET_EXPORT void cronetBidirectionalStreamOnSucceed(bidirectional_stream* stream);
// extern CRONET_EXPORT void cronetBidirectionalStreamOnFailed(bidirectional_stream* stream, int net_error);
// extern CRONET_EXPORT void cronetBidirectionalStreamOnCanceled(bidirectional_stream* stream);
import "C"

import (
	"unsafe"
)

var bidirectionalStreamCallbackCGO C.bidirectional_stream_callback

func init() {
	bidirectionalStreamCallbackCGO.on_stream_ready = (*[0]byte)(C.cronetBidirectionalStreamOnStreamReady)
	bidirectionalStreamCallbackCGO.on_response_headers_received = (*[0]byte)(C.cronetBidirectionalStreamOnResponseHeadersReceived)
	bidirectionalStreamCallbackCGO.on_read_completed = (*[0]byte)(C.cronetBidirectionalStreamOnReadCompleted)
	bidirectionalStreamCallbackCGO.on_write_completed = (*[0]byte)(C.cronetBidirectionalStreamOnWriteCompleted)
	bidirectionalStreamCallbackCGO.on_response_trailers_received = (*[0]byte)(C.cronetBidirectionalStreamOnResponseTrailersReceived)
	bidirectionalStreamCallbackCGO.on_succeded = (*[0]byte)(C.cronetBidirectionalStreamOnSucceed)
	bidirectionalStreamCallbackCGO.on_failed = (*[0]byte)(C.cronetBidirectionalStreamOnFailed)
	bidirectionalStreamCallbackCGO.on_canceled = (*[0]byte)(C.cronetBidirectionalStreamOnCanceled)
}

func instanceOfBidirectionalStream(stream *C.bidirectional_stream) BidirectionalStreamCallback {
	return instanceOfBidirectionalStreamCallback(uintptr(unsafe.Pointer(stream)))
}

func cBidirectionalStreamToGo(stream *C.bidirectional_stream) BidirectionalStream {
	return BidirectionalStream{uintptr(unsafe.Pointer(stream))}
}

//export cronetBidirectionalStreamOnStreamReady
func cronetBidirectionalStreamOnStreamReady(stream *C.bidirectional_stream) {
	callback := instanceOfBidirectionalStream(stream)
	if callback == nil {
		return
	}
	callback.OnStreamReady(cBidirectionalStreamToGo(stream))
}

//export cronetBidirectionalStreamOnResponseHeadersReceived
func cronetBidirectionalStreamOnResponseHeadersReceived(stream *C.bidirectional_stream, headers *C.bidirectional_stream_header_array, negotiatedProtocol *C.char) {
	callback := instanceOfBidirectionalStream(stream)
	if callback == nil {
		return
	}
	var headerMap map[string]string
	if headers != nil && headers.count > 0 {
		headerMap = make(map[string]string, int(headers.count))
		headersSlice := unsafe.Slice(headers.headers, int(headers.count))
		for _, header := range headersSlice {
			key := C.GoString(header.key)
			if len(key) == 0 {
				continue
			}
			headerMap[key] = C.GoString(header.value)
		}
	}
	callback.OnResponseHeadersReceived(cBidirectionalStreamToGo(stream), headerMap, C.GoString(negotiatedProtocol))
}

//export cronetBidirectionalStreamOnReadCompleted
func cronetBidirectionalStreamOnReadCompleted(stream *C.bidirectional_stream, data *C.char, bytesRead C.int) {
	callback := instanceOfBidirectionalStream(stream)
	if callback == nil {
		return
	}
	callback.OnReadCompleted(cBidirectionalStreamToGo(stream), int(bytesRead))
}

//export cronetBidirectionalStreamOnWriteCompleted
func cronetBidirectionalStreamOnWriteCompleted(stream *C.bidirectional_stream, data *C.char) {
	callback := instanceOfBidirectionalStream(stream)
	if callback == nil {
		return
	}
	callback.OnWriteCompleted(cBidirectionalStreamToGo(stream))
}

//export cronetBidirectionalStreamOnResponseTrailersReceived
func cronetBidirectionalStreamOnResponseTrailersReceived(stream *C.bidirectional_stream, trailers *C.bidirectional_stream_header_array) {
	callback := instanceOfBidirectionalStream(stream)
	if callback == nil {
		return
	}
	var trailersMap map[string]string
	if trailers != nil && trailers.count > 0 {
		trailersMap = make(map[string]string, int(trailers.count))
		headersSlice := unsafe.Slice(trailers.headers, int(trailers.count))
		for _, header := range headersSlice {
			key := C.GoString(header.key)
			if len(key) == 0 {
				continue
			}
			trailersMap[key] = C.GoString(header.value)
		}
	}
	callback.OnResponseTrailersReceived(cBidirectionalStreamToGo(stream), trailersMap)
}

//export cronetBidirectionalStreamOnSucceed
func cronetBidirectionalStreamOnSucceed(stream *C.bidirectional_stream) {
	callback := instanceOfBidirectionalStream(stream)
	if callback == nil {
		return
	}
	callback.OnSucceeded(cBidirectionalStreamToGo(stream))
	cleanupBidirectionalStream(uintptr(unsafe.Pointer(stream)))
}

//export cronetBidirectionalStreamOnFailed
func cronetBidirectionalStreamOnFailed(stream *C.bidirectional_stream, netError C.int) {
	callback := instanceOfBidirectionalStream(stream)
	if callback == nil {
		return
	}
	callback.OnFailed(cBidirectionalStreamToGo(stream), int(netError))
	cleanupBidirectionalStream(uintptr(unsafe.Pointer(stream)))
}

//export cronetBidirectionalStreamOnCanceled
func cronetBidirectionalStreamOnCanceled(stream *C.bidirectional_stream) {
	callback := instanceOfBidirectionalStream(stream)
	if callback == nil {
		return
	}
	callback.OnCanceled(cBidirectionalStreamToGo(stream))
	cleanupBidirectionalStream(uintptr(unsafe.Pointer(stream)))
}
