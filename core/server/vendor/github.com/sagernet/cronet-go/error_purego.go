//go:build with_purego

package cronet

import (
	"github.com/sagernet/cronet-go/internal/cronet"
)

func NewError() Error {
	return Error{cronet.ErrorCreate()}
}

func (e Error) Destroy() {
	cronet.ErrorDestroy(e.ptr)
}

func (e Error) ErrorCode() ErrorCode {
	return ErrorCode(cronet.ErrorErrorCodeGet(e.ptr))
}

func (e Error) Message() string {
	return cronet.ErrorMessageGet(e.ptr)
}

func (e Error) InternalErrorCode() int {
	return int(cronet.ErrorInternalErrorCodeGet(e.ptr))
}

func (e Error) Retryable() bool {
	return cronet.ErrorImmediatelyRetryableGet(e.ptr)
}

func (e Error) QuicDetailedErrorCode() int {
	return int(cronet.ErrorQuicDetailedErrorCodeGet(e.ptr))
}

func (e Error) SetErrorCode(code ErrorCode) {
	cronet.ErrorErrorCodeSet(e.ptr, int32(code))
}

func (e Error) SetMessage(message string) {
	cronet.ErrorMessageSet(e.ptr, message)
}

func (e Error) SetInternalErrorCode(code int32) {
	cronet.ErrorInternalErrorCodeSet(e.ptr, code)
}

func (e Error) SetRetryable(retryable bool) {
	cronet.ErrorImmediatelyRetryableSet(e.ptr, retryable)
}

func (e Error) SetQuicDetailedErrorCode(code int32) {
	cronet.ErrorQuicDetailedErrorCodeSet(e.ptr, code)
}
