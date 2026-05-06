//go:build with_purego

package cronet

import (
	"runtime"
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (e Engine) StreamEngine() StreamEngine {
	return StreamEngine{cronet.EngineGetStreamEngine(e.ptr)}
}

// CreateStream creates a new stream object that uses |engine| and |callback|.
func (e StreamEngine) CreateStream(callback BidirectionalStreamCallback) BidirectionalStream {
	if callback == nil {
		panic("nil bidirectional stream callback")
	}
	ptr := cronet.BidirectionalStreamCreate(e.ptr, 0, uintptr(unsafe.Pointer(&bsCallbackStructPurego)))
	bidirectionalStreamAccess.Lock()
	bidirectionalStreamMap[ptr] = &bidirectionalStreamEntry{callback: callback}
	bidirectionalStreamAccess.Unlock()
	return BidirectionalStream{ptr}
}

// Destroy destroys stream object. The destroy operation is asynchronous -
// callbacks may still be invoked after this returns. The stream is marked
// as destroyed and callbacks will silently return.
func (s BidirectionalStream) Destroy() bool {
	bidirectionalStreamAccess.RLock()
	entry := bidirectionalStreamMap[s.ptr]
	bidirectionalStreamAccess.RUnlock()
	if entry != nil {
		entry.destroyed.Store(true)
	}
	return cronet.BidirectionalStreamDestroy(s.ptr) == 0
}

// Start starts the stream by sending request to |url| using |method| and |headers|.
func (c BidirectionalStream) Start(method string, url string, headers map[string]string, priority int, endOfStream bool) bool {
	var headerArrayPtr uintptr
	var cStringBacking [][]byte // Keep C string backing slices alive

	headerLen := len(headers)
	if headerLen > 0 {
		// Allocate header structs
		headerStructs := make([]bidirectionalStreamHeader, headerLen)
		var index int
		for key, value := range headers {
			keyPtr, keyBacking := cronet.CString(key)
			valuePtr, valueBacking := cronet.CString(value)
			cStringBacking = append(cStringBacking, keyBacking, valueBacking)
			headerStructs[index].key = keyPtr
			headerStructs[index].value = valuePtr
			index++
		}

		headerArray := bidirectionalStreamHeaderArray{
			count:    uintptr(headerLen),
			capacity: uintptr(headerLen),
			headers:  uintptr(unsafe.Pointer(&headerStructs[0])),
		}
		headerArrayPtr = uintptr(unsafe.Pointer(&headerArray))

		result := cronet.BidirectionalStreamStart(c.ptr, url, int32(priority), method, headerArrayPtr, endOfStream) == 0

		// Keep all backing slices alive until after the call
		runtime.KeepAlive(cStringBacking)
		runtime.KeepAlive(headerStructs)
		runtime.KeepAlive(headerArray)

		return result
	}

	return cronet.BidirectionalStreamStart(c.ptr, url, int32(priority), method, 0, endOfStream) == 0
}

func (s BidirectionalStream) DisableAutoFlush(disable bool) {
	cronet.BidirectionalStreamDisableAutoFlush(s.ptr, disable)
}

func (s BidirectionalStream) DelayRequestHeadersUntilFlush(delay bool) {
	cronet.BidirectionalStreamDelayRequestHeadersUntilFlush(s.ptr, delay)
}

func (s BidirectionalStream) Read(buffer []byte) int {
	if len(buffer) == 0 {
		return int(cronet.BidirectionalStreamRead(s.ptr, 0, 0))
	}
	return int(cronet.BidirectionalStreamRead(s.ptr, uintptr(unsafe.Pointer(&buffer[0])), int32(len(buffer))))
}

func (s BidirectionalStream) Write(buffer []byte, endOfStream bool) int {
	if len(buffer) == 0 {
		return int(cronet.BidirectionalStreamWrite(s.ptr, 0, 0, endOfStream))
	}
	return int(cronet.BidirectionalStreamWrite(s.ptr, uintptr(unsafe.Pointer(&buffer[0])), int32(len(buffer)), endOfStream))
}

func (s BidirectionalStream) Flush() {
	cronet.BidirectionalStreamFlush(s.ptr)
}

func (s BidirectionalStream) Cancel() {
	cronet.BidirectionalStreamCancel(s.ptr)
}
