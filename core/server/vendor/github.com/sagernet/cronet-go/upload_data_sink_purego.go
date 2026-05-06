//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func (s UploadDataSink) Destroy() {
	cronet.UploadDataSinkDestroy(s.ptr)
}

func (s UploadDataSink) SetClientContext(context unsafe.Pointer) {
	cronet.UploadDataSinkSetClientContext(s.ptr, uintptr(context))
}

func (s UploadDataSink) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.UploadDataSinkGetClientContext(s.ptr))
}

func (s UploadDataSink) OnReadSucceeded(bytesRead int64, finalChunk bool) {
	cronet.UploadDataSinkOnReadSucceeded(s.ptr, uint64(bytesRead), finalChunk)
}

func (s UploadDataSink) OnReadError(message string) {
	cronet.UploadDataSinkOnReadError(s.ptr, message)
}

func (s UploadDataSink) OnRewindSucceeded() {
	cronet.UploadDataSinkOnRewindSucceeded(s.ptr)
}

func (s UploadDataSink) OnRewindError(message string) {
	cronet.UploadDataSinkOnRewindError(s.ptr, message)
}
