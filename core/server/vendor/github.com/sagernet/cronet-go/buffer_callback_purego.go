//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (c BufferCallback) Destroy() {
	c.destroy()
	cronet.BufferCallbackDestroy(c.ptr)
}

func (c BufferCallback) SetClientContext(context unsafe.Pointer) {
	cronet.BufferCallbackSetClientContext(c.ptr, uintptr(context))
}

func (c BufferCallback) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.BufferCallbackGetClientContext(c.ptr))
}
