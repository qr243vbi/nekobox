//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func (c BufferCallback) Destroy() {
	c.destroy()
	C.Cronet_BufferCallback_Destroy(C.Cronet_BufferCallbackPtr(unsafe.Pointer(c.ptr)))
}

func (c BufferCallback) SetClientContext(context unsafe.Pointer) {
	C.Cronet_BufferCallback_SetClientContext(C.Cronet_BufferCallbackPtr(unsafe.Pointer(c.ptr)), C.Cronet_ClientContext(context))
}

func (c BufferCallback) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_BufferCallback_GetClientContext(C.Cronet_BufferCallbackPtr(unsafe.Pointer(c.ptr))))
}
