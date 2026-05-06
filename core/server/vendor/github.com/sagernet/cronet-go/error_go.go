package cronet

type ErrorGo struct {
	ErrorCode             ErrorCode
	Message               string
	InternalErrorCode     int
	Retryable             bool
	QuicDetailedErrorCode int
}

func (e *ErrorGo) Error() string {
	return e.Message
}

func (e *ErrorGo) Timeout() bool {
	return e.ErrorCode == ErrorCodeErrorConnectionTimedOut
}

func (e *ErrorGo) Temporary() bool {
	return e.Retryable
}

func ErrorFromError(error Error) *ErrorGo {
	return &ErrorGo{
		ErrorCode:             error.ErrorCode(),
		Message:               error.Message(),
		InternalErrorCode:     error.InternalErrorCode(),
		Retryable:             error.Retryable(),
		QuicDetailedErrorCode: error.QuicDetailedErrorCode(),
	}
}
