//go:build with_purego

package cronet

import (
	"unsafe"

	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewURLRequestFinishedInfo() RequestFinishedInfo {
	return RequestFinishedInfo{cronet.RequestFinishedInfoCreate()}
}

func (i RequestFinishedInfo) Destroy() {
	cronet.RequestFinishedInfoDestroy(i.ptr)
}

func (i RequestFinishedInfo) Metrics() Metrics {
	return Metrics{cronet.RequestFinishedInfoMetricsGet(i.ptr)}
}

func (i RequestFinishedInfo) AnnotationSize() int {
	return int(cronet.RequestFinishedInfoAnnotationsSize(i.ptr))
}

func (i RequestFinishedInfo) AnnotationAt(index int) unsafe.Pointer {
	return unsafe.Pointer(cronet.RequestFinishedInfoAnnotationsAt(i.ptr, uint32(index)))
}

func (i RequestFinishedInfo) FinishedReason() URLRequestFinishedInfoFinishedReason {
	return URLRequestFinishedInfoFinishedReason(cronet.RequestFinishedInfoFinishedReasonGet(i.ptr))
}
