//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import (
	"unsafe"
)

func NewBuffer() Buffer {
	return Buffer{uintptr(unsafe.Pointer(C.Cronet_Buffer_Create()))}
}

func (b Buffer) Destroy() {
	C.Cronet_Buffer_Destroy(C.Cronet_BufferPtr(unsafe.Pointer(b.ptr)))
}

// InitWithDataAndCallback initialize Buffer with raw buffer |data| of |size| allocated by the app.
// The |callback| is invoked when buffer is destroyed.
func (b Buffer) InitWithDataAndCallback(data []byte, callback BufferCallback) {
	C.Cronet_Buffer_InitWithDataAndCallback(C.Cronet_BufferPtr(unsafe.Pointer(b.ptr)), C.Cronet_RawDataPtr(unsafe.Pointer(&data[0])), C.uint64_t(len(data)), C.Cronet_BufferCallbackPtr(unsafe.Pointer(callback.ptr)))
}

// InitWithAlloc initialize Buffer by allocating buffer of |size|.
// The content of allocated data is not initialized.
func (b Buffer) InitWithAlloc(size int64) {
	C.Cronet_Buffer_InitWithAlloc(C.Cronet_BufferPtr(unsafe.Pointer(b.ptr)), C.uint64_t(size))
}

// Size return size of data owned by this buffer.
func (b Buffer) Size() int64 {
	return int64(C.Cronet_Buffer_GetSize(C.Cronet_BufferPtr(unsafe.Pointer(b.ptr))))
}

// Data return raw pointer to |data| owned by this buffer.
func (b Buffer) Data() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_Buffer_GetData(C.Cronet_BufferPtr(unsafe.Pointer(b.ptr))))
}

func (b Buffer) DataSlice() []byte {
	size := b.Size()
	if size == 0 {
		return nil
	}
	return unsafe.Slice((*byte)(b.Data()), size)
}

func (b Buffer) SetClientContext(context unsafe.Pointer) {
	C.Cronet_Buffer_SetClientContext(C.Cronet_BufferPtr(unsafe.Pointer(b.ptr)), C.Cronet_ClientContext(context))
}

func (b Buffer) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_Buffer_GetClientContext(C.Cronet_BufferPtr(unsafe.Pointer(b.ptr))))
}
