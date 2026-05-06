//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func (e Executor) Execute(command Runnable) {
	C.Cronet_Executor_Execute(C.Cronet_ExecutorPtr(unsafe.Pointer(e.ptr)), C.Cronet_RunnablePtr(unsafe.Pointer(command.ptr)))
}

func (e Executor) SetClientContext(context unsafe.Pointer) {
	C.Cronet_Executor_SetClientContext(C.Cronet_ExecutorPtr(unsafe.Pointer(e.ptr)), C.Cronet_ClientContext(context))
}

func (e Executor) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_Executor_GetClientContext(C.Cronet_ExecutorPtr(unsafe.Pointer(e.ptr))))
}
