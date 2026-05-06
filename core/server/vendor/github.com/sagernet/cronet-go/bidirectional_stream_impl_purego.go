//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"

	"github.com/ebitengine/purego"
)

type bidirectionalStreamCallbackStruct struct {
	onStreamReady              uintptr
	onResponseHeadersReceived  uintptr
	onReadCompleted            uintptr
	onWriteCompleted           uintptr
	onResponseTrailersReceived uintptr
	onSucceeded                uintptr
	onFailed                   uintptr
	onCanceled                 uintptr
}

type bidirectionalStreamHeaderArray struct {
	count    uintptr
	capacity uintptr
	headers  uintptr
}

type bidirectionalStreamHeader struct {
	key   uintptr // const char*
	value uintptr // const char*
}

var bsCallbackStructPurego bidirectionalStreamCallbackStruct

func init() {
	bsCallbackStructPurego.onStreamReady = purego.NewCallback(bsOnStreamReadyCallback)
	bsCallbackStructPurego.onResponseHeadersReceived = purego.NewCallback(bsOnResponseHeadersReceivedCallback)
	bsCallbackStructPurego.onReadCompleted = purego.NewCallback(bsOnReadCompletedCallback)
	bsCallbackStructPurego.onWriteCompleted = purego.NewCallback(bsOnWriteCompletedCallback)
	bsCallbackStructPurego.onResponseTrailersReceived = purego.NewCallback(bsOnResponseTrailersReceivedCallback)
	bsCallbackStructPurego.onSucceeded = purego.NewCallback(bsOnSucceededCallback)
	bsCallbackStructPurego.onFailed = purego.NewCallback(bsOnFailedCallback)
	bsCallbackStructPurego.onCanceled = purego.NewCallback(bsOnCanceledCallback)
}

func bsOnStreamReadyCallback(stream uintptr) uintptr {
	cb := instanceOfBidirectionalStreamCallback(stream)
	if cb == nil {
		return 0
	}
	cb.OnStreamReady(BidirectionalStream{stream})
	return 0
}

func bsOnResponseHeadersReceivedCallback(stream, headers, negotiatedProtocol uintptr) uintptr {
	cb := instanceOfBidirectionalStreamCallback(stream)
	if cb == nil {
		return 0
	}
	headerMap := parseHeaderArray(headers)
	cb.OnResponseHeadersReceived(BidirectionalStream{stream}, headerMap, cronet.GoString(negotiatedProtocol))
	return 0
}

func bsOnReadCompletedCallback(stream, data uintptr, bytesRead int32) uintptr {
	cb := instanceOfBidirectionalStreamCallback(stream)
	if cb == nil {
		return 0
	}
	cb.OnReadCompleted(BidirectionalStream{stream}, int(bytesRead))
	return 0
}

func bsOnWriteCompletedCallback(stream, data uintptr) uintptr {
	cb := instanceOfBidirectionalStreamCallback(stream)
	if cb == nil {
		return 0
	}
	cb.OnWriteCompleted(BidirectionalStream{stream})
	return 0
}

func bsOnResponseTrailersReceivedCallback(stream, trailers uintptr) uintptr {
	cb := instanceOfBidirectionalStreamCallback(stream)
	if cb == nil {
		return 0
	}
	trailerMap := parseHeaderArray(trailers)
	cb.OnResponseTrailersReceived(BidirectionalStream{stream}, trailerMap)
	return 0
}

func bsOnSucceededCallback(stream uintptr) uintptr {
	cb := instanceOfBidirectionalStreamCallback(stream)
	if cb == nil {
		return 0
	}
	cb.OnSucceeded(BidirectionalStream{stream})
	cleanupBidirectionalStream(stream)
	return 0
}

func bsOnFailedCallback(stream uintptr, netError int32) uintptr {
	cb := instanceOfBidirectionalStreamCallback(stream)
	if cb == nil {
		return 0
	}
	cb.OnFailed(BidirectionalStream{stream}, int(netError))
	cleanupBidirectionalStream(stream)
	return 0
}

func bsOnCanceledCallback(stream uintptr) uintptr {
	cb := instanceOfBidirectionalStreamCallback(stream)
	if cb == nil {
		return 0
	}
	cb.OnCanceled(BidirectionalStream{stream})
	cleanupBidirectionalStream(stream)
	return 0
}

func parseHeaderArray(ptr uintptr) map[string]string {
	if ptr == 0 {
		return nil
	}
	arr := (*bidirectionalStreamHeaderArray)(unsafe.Pointer(ptr))
	count := int(arr.count)
	if count == 0 {
		return nil
	}
	result := make(map[string]string, count)
	headers := unsafe.Slice((*bidirectionalStreamHeader)(unsafe.Pointer(arr.headers)), count)
	for _, h := range headers {
		key := cronet.GoString(h.key)
		if key == "" {
			continue
		}
		result[key] = cronet.GoString(h.value)
	}
	return result
}
