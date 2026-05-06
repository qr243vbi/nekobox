//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewURLRequest() URLRequest {
	return URLRequest{cronet.UrlRequestCreate()}
}

func (r URLRequest) Destroy() {
	cronet.UrlRequestDestroy(r.ptr)
}

func (r URLRequest) SetClientContext(context unsafe.Pointer) {
	cronet.UrlRequestSetClientContext(r.ptr, uintptr(context))
}

func (r URLRequest) ClientContext() unsafe.Pointer {
	return unsafe.Pointer(cronet.UrlRequestGetClientContext(r.ptr))
}

func (r URLRequest) InitWithParams(engine Engine, url string, params URLRequestParams, callback URLRequestCallback, executor Executor) Result {
	return Result(cronet.UrlRequestInitWithParams(r.ptr, engine.ptr, url, params.ptr, callback.ptr, executor.ptr))
}

func (r URLRequest) Start() Result {
	return Result(cronet.UrlRequestStart(r.ptr))
}

func (r URLRequest) FollowRedirect() Result {
	return Result(cronet.UrlRequestFollowRedirect(r.ptr))
}

func (r URLRequest) Read(buffer Buffer) Result {
	return Result(cronet.UrlRequestRead(r.ptr, buffer.ptr))
}

func (r URLRequest) Cancel() {
	cronet.UrlRequestCancel(r.ptr)
}

func (r URLRequest) IsDone() bool {
	return cronet.UrlRequestIsDone(r.ptr)
}

func (r URLRequest) GetStatus(listener URLRequestStatusListener) {
	cronet.UrlRequestGetStatus(r.ptr, listener.ptr)
}
