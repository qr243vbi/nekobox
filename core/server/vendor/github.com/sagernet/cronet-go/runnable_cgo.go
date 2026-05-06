//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func (r Runnable) Run() {
	C.Cronet_Runnable_Run(C.Cronet_RunnablePtr(unsafe.Pointer(r.ptr)))
}

func (r Runnable) SetClientContext(context unsafe.Pointer) {
	C.Cronet_Runnable_SetClientContext(C.Cronet_RunnablePtr(unsafe.Pointer(r.ptr)), C.Cronet_ClientContext(context))
}

func (r Runnable) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_Runnable_GetClientContext(C.Cronet_RunnablePtr(unsafe.Pointer(r.ptr))))
}
