//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (l URLRequestFinishedInfoListener) Destroy() {
	l.destroy()
	cronet.RequestFinishedInfoListenerDestroy(l.ptr)
}

func (l URLRequestFinishedInfoListener) SetClientContext(context unsafe.Pointer) {
	cronet.RequestFinishedInfoListenerSetClientContext(l.ptr, uintptr(context))
}

func (l URLRequestFinishedInfoListener) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.RequestFinishedInfoListenerGetClientContext(l.ptr))
}
