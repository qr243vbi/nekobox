//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewBuffer() Buffer {
	return Buffer{cronet.BufferCreate()}
}

func (b Buffer) Destroy() {
	cronet.BufferDestroy(b.ptr)
}

func (b Buffer) SetClientContext(context unsafe.Pointer) {
	cronet.BufferSetClientContext(b.ptr, uintptr(context))
}

func (b Buffer) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.BufferGetClientContext(b.ptr))
}

// InitWithDataAndCallback initializes Buffer with raw buffer |data| allocated by the app.
// The |callback| is invoked when buffer is destroyed.
func (b Buffer) InitWithDataAndCallback(data []byte, callback BufferCallback) {
	if len(data) == 0 {
		cronet.BufferInitWithDataAndCallback(b.ptr, 0, 0, callback.ptr)
		return
	}
	cronet.BufferInitWithDataAndCallback(b.ptr, uintptr(unsafe.Pointer(&data[0])), uint64(len(data)), callback.ptr)
}

// InitWithAlloc initializes Buffer by allocating buffer of |size|.
// The content of allocated data is not initialized.
func (b Buffer) InitWithAlloc(size int64) {
	cronet.BufferInitWithAlloc(b.ptr, uint64(size))
}

// Size returns size of data owned by this buffer.
func (b Buffer) Size() int64 {
	return int64(cronet.BufferGetSize(b.ptr))
}

// Data returns raw pointer to |data| owned by this buffer.
func (b Buffer) Data() unsafe.Pointer {
	return unsafe.Pointer(cronet.BufferGetData(b.ptr))
}

func (b Buffer) DataSlice() []byte {
	size := b.Size()
	if size == 0 {
		return nil
	}
	return unsafe.Slice((*byte)(b.Data()), size)
}
