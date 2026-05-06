//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewURLRequestParams() URLRequestParams {
	return URLRequestParams{cronet.UrlRequestParamsCreate()}
}

func (p URLRequestParams) Destroy() {
	cronet.UrlRequestParamsDestroy(p.ptr)
}

func (p URLRequestParams) SetMethod(method string) {
	cronet.UrlRequestParamsHttpMethodSet(p.ptr, method)
}

func (p URLRequestParams) Method() string {
	return cronet.UrlRequestParamsHttpMethodGet(p.ptr)
}

func (p URLRequestParams) AddHeader(header HTTPHeader) {
	cronet.UrlRequestParamsRequestHeadersAdd(p.ptr, header.ptr)
}

func (p URLRequestParams) HeaderSize() int {
	return int(cronet.UrlRequestParamsRequestHeadersSize(p.ptr))
}

func (p URLRequestParams) HeaderAt(index int) HTTPHeader {
	return HTTPHeader{cronet.UrlRequestParamsRequestHeadersAt(p.ptr, uint32(index))}
}

func (p URLRequestParams) ClearHeaders() {
	cronet.UrlRequestParamsRequestHeadersClear(p.ptr)
}

func (p URLRequestParams) SetDisableCache(disable bool) {
	cronet.UrlRequestParamsDisableCacheSet(p.ptr, disable)
}

func (p URLRequestParams) DisableCache() bool {
	return cronet.UrlRequestParamsDisableCacheGet(p.ptr)
}

func (p URLRequestParams) SetPriority(priority URLRequestParamsRequestPriority) {
	cronet.UrlRequestParamsPrioritySet(p.ptr, int32(priority))
}

func (p URLRequestParams) Priority() URLRequestParamsRequestPriority {
	return URLRequestParamsRequestPriority(cronet.UrlRequestParamsPriorityGet(p.ptr))
}

func (p URLRequestParams) SetUploadDataProvider(provider UploadDataProvider) {
	cronet.UrlRequestParamsUploadDataProviderSet(p.ptr, provider.ptr)
}

func (p URLRequestParams) UploadDataProvider() UploadDataProvider {
	return UploadDataProvider{cronet.UrlRequestParamsUploadDataProviderGet(p.ptr)}
}

func (p URLRequestParams) SetUploadDataExecutor(executor Executor) {
	cronet.UrlRequestParamsUploadDataProviderExecutorSet(p.ptr, executor.ptr)
}

func (p URLRequestParams) UploadDataExecutor() Executor {
	return Executor{cronet.UrlRequestParamsUploadDataProviderExecutorGet(p.ptr)}
}

func (p URLRequestParams) SetAllowDirectExecutor(allow bool) {
	cronet.UrlRequestParamsAllowDirectExecutorSet(p.ptr, allow)
}

func (p URLRequestParams) AllocDirectExecutor() bool {
	return cronet.UrlRequestParamsAllowDirectExecutorGet(p.ptr)
}

func (p URLRequestParams) AddAnnotation(annotation unsafe.Pointer) {
	cronet.UrlRequestParamsAnnotationsAdd(p.ptr, uintptr(annotation))
}

func (p URLRequestParams) AnnotationSize() int {
	return int(cronet.UrlRequestParamsAnnotationsSize(p.ptr))
}

func (p URLRequestParams) AnnotationAt(index int) unsafe.Pointer {
	return unsafe.Pointer(cronet.UrlRequestParamsAnnotationsAt(p.ptr, uint32(index)))
}

func (p URLRequestParams) ClearAnnotations() {
	cronet.UrlRequestParamsAnnotationsClear(p.ptr)
}

func (p URLRequestParams) SetRequestFinishedListener(listener URLRequestFinishedInfoListener) {
	cronet.UrlRequestParamsRequestFinishedListenerSet(p.ptr, listener.ptr)
}

func (p URLRequestParams) RequestFinishedListener() URLRequestFinishedInfoListener {
	return URLRequestFinishedInfoListener{cronet.UrlRequestParamsRequestFinishedListenerGet(p.ptr)}
}

func (p URLRequestParams) SetRequestFinishedExecutor(executor Executor) {
	cronet.UrlRequestParamsRequestFinishedExecutorSet(p.ptr, executor.ptr)
}

func (p URLRequestParams) RequestFinishedExecutor() Executor {
	return Executor{cronet.UrlRequestParamsRequestFinishedExecutorGet(p.ptr)}
}

func (p URLRequestParams) SetIdempotency(idempotency URLRequestParamsIdempotency) {
	cronet.UrlRequestParamsIdempotencySet(p.ptr, int32(idempotency))
}

func (p URLRequestParams) Idempotency() URLRequestParamsIdempotency {
	return URLRequestParamsIdempotency(cronet.UrlRequestParamsIdempotencyGet(p.ptr))
}
