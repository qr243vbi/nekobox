//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (r Runnable) SetClientContext(context unsafe.Pointer) {
	cronet.RunnableSetClientContext(r.ptr, uintptr(context))
}

func (r Runnable) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.RunnableGetClientContext(r.ptr))
}

func (r Runnable) Run() {
	cronet.RunnableRun(r.ptr)
}
