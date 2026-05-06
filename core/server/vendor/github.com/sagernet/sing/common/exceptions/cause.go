package exceptions

type causeError struct {
	message string
	cause   error
}

func (e *causeError) Error() string {
	return e.message + ": " + e.cause.Error()
}

func (e *causeError) Unwrap() error {
	return e.cause
}

type causeError1 struct {
	error
	cause error
}

func (e *causeError1) Error() string {
	return e.error.Error() + ": " + e.cause.Error()
}

func (e *causeError1) Unwrap() []error {
	return []error{e.error, e.cause}
}
