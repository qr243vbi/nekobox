//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (p UploadDataProvider) SetClientContext(context unsafe.Pointer) {
	cronet.UploadDataProviderSetClientContext(p.ptr, uintptr(context))
}

func (p UploadDataProvider) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.UploadDataProviderGetClientContext(p.ptr))
}
