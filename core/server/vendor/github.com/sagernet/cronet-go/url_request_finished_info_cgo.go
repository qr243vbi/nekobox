//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

// Note: RequestFinishedInfo is used in types.go, but the C type uses different naming.
// The Go type is defined in types.go as RequestFinishedInfo (without URL prefix for internal consistency).

func NewURLRequestFinishedInfo() RequestFinishedInfo {
	return RequestFinishedInfo{uintptr(unsafe.Pointer(C.Cronet_RequestFinishedInfo_Create()))}
}

func (i RequestFinishedInfo) Destroy() {
	C.Cronet_RequestFinishedInfo_Destroy(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr)))
}

// Metrics
// Metrics collected for this request.
func (i RequestFinishedInfo) Metrics() Metrics {
	return Metrics{uintptr(unsafe.Pointer(C.Cronet_RequestFinishedInfo_metrics_get(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr)))))}
}

// AnnotationSize
// The objects that the caller has supplied when initiating the request,
// using URLRequestParams.AddAnnotation
//
// Annotations can be used to associate a RequestFinishedInfo with
// the original request or type of request.
func (i RequestFinishedInfo) AnnotationSize() int {
	return int(C.Cronet_RequestFinishedInfo_annotations_size(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr))))
}

func (i RequestFinishedInfo) AnnotationAt(index int) unsafe.Pointer {
	return unsafe.Pointer(C.Cronet_RequestFinishedInfo_annotations_at(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr)), C.uint32_t(index)))
}

// FinishedReason
// Returns the reason why the request finished.
func (i RequestFinishedInfo) FinishedReason() URLRequestFinishedInfoFinishedReason {
	return URLRequestFinishedInfoFinishedReason(C.Cronet_RequestFinishedInfo_finished_reason_get(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr))))
}

func (i RequestFinishedInfo) SetMetrics(metrics Metrics) {
	C.Cronet_RequestFinishedInfo_metrics_set(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr)), C.Cronet_MetricsPtr(unsafe.Pointer(metrics.ptr)))
}

func (i RequestFinishedInfo) AddAnnotation(annotation unsafe.Pointer) {
	C.Cronet_RequestFinishedInfo_annotations_add(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr)), C.Cronet_RawDataPtr(annotation))
}

func (i RequestFinishedInfo) ClearAnnotations() {
	C.Cronet_RequestFinishedInfo_annotations_clear(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr)))
}

func (i RequestFinishedInfo) SetFinishedReason(reason URLRequestFinishedInfoFinishedReason) {
	C.Cronet_RequestFinishedInfo_finished_reason_set(C.Cronet_RequestFinishedInfoPtr(unsafe.Pointer(i.ptr)), C.Cronet_RequestFinishedInfo_FINISHED_REASON(reason))
}
