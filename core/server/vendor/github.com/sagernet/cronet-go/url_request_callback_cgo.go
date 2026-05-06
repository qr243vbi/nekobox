//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func (c URLRequestCallback) SetClientContext(context unsafe.Pointer) {
	C.Cronet_UrlRequestCallback_SetClientContext(C.Cronet_UrlRequestCallbackPtr(unsafe.Pointer(c.ptr)), C.Cronet_ClientContext(context))
}

func (c URLRequestCallback) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_UrlRequestCallback_GetClientContext(C.Cronet_UrlRequestCallbackPtr(unsafe.Pointer(c.ptr))))
}
