//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func (l URLRequestStatusListener) SetClientContext(context unsafe.Pointer) {
	C.Cronet_UrlRequestStatusListener_SetClientContext(C.Cronet_UrlRequestStatusListenerPtr(unsafe.Pointer(l.ptr)), C.Cronet_ClientContext(context))
}

func (l URLRequestStatusListener) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_UrlRequestStatusListener_GetClientContext(C.Cronet_UrlRequestStatusListenerPtr(unsafe.Pointer(l.ptr))))
}
