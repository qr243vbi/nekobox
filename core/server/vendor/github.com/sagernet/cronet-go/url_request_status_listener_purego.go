//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (l URLRequestStatusListener) Destroy() {
	l.destroy()
	cronet.UrlRequestStatusListenerDestroy(l.ptr)
}

func (l URLRequestStatusListener) SetClientContext(context unsafe.Pointer) {
	cronet.UrlRequestStatusListenerSetClientContext(l.ptr, uintptr(context))
}

func (l URLRequestStatusListener) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.UrlRequestStatusListenerGetClientContext(l.ptr))
}
