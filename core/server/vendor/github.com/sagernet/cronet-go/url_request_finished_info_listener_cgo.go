//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func (l URLRequestFinishedInfoListener) SetClientContext(context unsafe.Pointer) {
	C.Cronet_RequestFinishedInfoListener_SetClientContext(C.Cronet_RequestFinishedInfoListenerPtr(unsafe.Pointer(l.ptr)), C.Cronet_ClientContext(context))
}

func (l URLRequestFinishedInfoListener) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_RequestFinishedInfoListener_GetClientContext(C.Cronet_RequestFinishedInfoListenerPtr(unsafe.Pointer(l.ptr))))
}
