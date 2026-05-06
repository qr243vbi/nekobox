package exceptions

type extendedError struct {
	message string
	cause   error
}

func (e *extendedError) Error() string {
	if e.cause == nil {
		return e.message
	}
	return e.cause.Error() + ": " + e.message
}

func (e *extendedError) Unwrap() error {
	return e.cause
}
