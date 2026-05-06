//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (e Executor) SetClientContext(context unsafe.Pointer) {
	cronet.ExecutorSetClientContext(e.ptr, uintptr(context))
}

func (e Executor) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.ExecutorGetClientContext(e.ptr))
}

func (e Executor) Execute(runnable Runnable) {
	cronet.ExecutorExecute(e.ptr, runnable.ptr)
}
