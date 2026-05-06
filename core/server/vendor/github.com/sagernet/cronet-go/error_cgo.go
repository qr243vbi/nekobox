//go:build !with_purego

package cronet

// #include <stdlib.h>
// #include <stdbool.h>
// #include <cronet_c.h>
import "C"

import "unsafe"

func NewError() Error {
	return Error{uintptr(unsafe.Pointer(C.Cronet_Error_Create()))}
}

func (e Error) Destroy() {
	C.Cronet_Error_Destroy(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr)))
}

// ErrorCode return the error code, one of ErrorCode values.
func (e Error) ErrorCode() ErrorCode {
	return ErrorCode(C.Cronet_Error_error_code_get(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr))))
}

// Message explaining the error.
func (e Error) Message() string {
	return C.GoString(C.Cronet_Error_message_get(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr))))
}

// InternalErrorCode is the cronet internal error code. This may provide more specific error
// diagnosis than ErrorCode(), but the constant values may change over time.
// See
// <a href=https://chromium.googlesource.com/chromium/src/+/main/net/base/net_error_list.h> here</a>
// for the latest list of values.
func (e Error) InternalErrorCode() int {
	return int(C.Cronet_Error_internal_error_code_get(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr))))
}

// Retryable |true| if retrying this request right away might succeed, |false|
// otherwise. For example, is |true| when ErrorCode() is ErrorCodeErrorNetworkChanged
// because trying the request might succeed using the new
// network configuration, but |false| when ErrorCode() is
// ErrorCodeErrorInternetDisconnected because retrying the request right away will
// encounter the same failure (instead retrying should be delayed until device regains
// network connectivity).
func (e Error) Retryable() bool {
	return bool(C.Cronet_Error_immediately_retryable_get(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr))))
}

// QuicDetailedErrorCode contains detailed <a href="https://www.chromium.org/quic">QUIC</a> error code from
// <a href="https://cs.chromium.org/search/?q=symbol:%5CbQuicErrorCode%5Cb">
// QuicErrorCode</a> when the ErrorCode() code is ErrorCodeErrorQuicProtocolFailed.
func (e Error) QuicDetailedErrorCode() int {
	return int(C.Cronet_Error_quic_detailed_error_code_get(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr))))
}

func (e Error) SetErrorCode(code ErrorCode) {
	C.Cronet_Error_error_code_set(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr)), C.Cronet_Error_ERROR_CODE(code))
}

func (e Error) SetMessage(message string) {
	cMessage := C.CString(message)
	C.Cronet_Error_message_set(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr)), cMessage)
	C.free(unsafe.Pointer(cMessage))
}

func (e Error) SetInternalErrorCode(code int32) {
	C.Cronet_Error_internal_error_code_set(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr)), C.int32_t(code))
}

func (e Error) SetRetryable(retryable bool) {
	C.Cronet_Error_immediately_retryable_set(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr)), C.bool(retryable))
}

func (e Error) SetQuicDetailedErrorCode(code int32) {
	C.Cronet_Error_quic_detailed_error_code_set(C.Cronet_ErrorPtr(unsafe.Pointer(e.ptr)), C.int32_t(code))
}
