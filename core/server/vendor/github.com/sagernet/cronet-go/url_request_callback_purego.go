//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (c URLRequestCallback) SetClientContext(context unsafe.Pointer) {
	cronet.UrlRequestCallbackSetClientContext(c.ptr, uintptr(context))
}

func (c URLRequestCallback) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.UrlRequestCallbackGetClientContext(c.ptr))
}
