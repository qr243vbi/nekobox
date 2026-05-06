//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func (p UploadDataProvider) SetClientContext(context unsafe.Pointer) {
	C.Cronet_UploadDataProvider_SetClientContext(C.Cronet_UploadDataProviderPtr(unsafe.Pointer(p.ptr)), C.Cronet_ClientContext(context))
}

func (p UploadDataProvider) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_UploadDataProvider_GetClientContext(C.Cronet_UploadDataProviderPtr(unsafe.Pointer(p.ptr))))
}
