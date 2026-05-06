package exceptions

import (
	"context"
	"errors"
	"io"
	"net"
	"net/http"
	"os"
	"syscall"
	_ "unsafe"

	F "github.com/sagernet/sing/common/format"
)

// Deprecated: wtf is this?
type Handler interface {
	NewError(ctx context.Context, err error)
}

type MultiError interface {
	Unwrap() []error
}

func New(message ...any) error {
	return errors.New(F.ToString(message...))
}

func Cause(cause error, message ...any) error {
	if cause == nil {
		panic("cause on an nil error")
	}
	return &causeError{F.ToString(message...), cause}
}

func Cause1(err error, cause error) error {
	if cause == nil {
		panic("cause on an nil error")
	}
	return &causeError1{err, cause}
}

func Extend(cause error, message ...any) error {
	if cause == nil {
		panic("extend on an nil error")
	}
	return &extendedError{F.ToString(message...), cause}
}

func IsClosedOrCanceled(err error) bool {
	return IsClosed(err) || IsCanceled(err) || IsTimeout(err)
}

func IsClosed(err error) bool {
	return IsMulti(err,
		io.EOF,
		net.ErrClosed,
		io.ErrClosedPipe,
		os.ErrClosed,
		syscall.EPIPE,
		syscall.ECONNRESET,
		syscall.ENOTCONN,
		http.ErrServerClosed,
	)
}

func IsCanceled(err error) bool {
	return IsMulti(err, context.Canceled, context.DeadlineExceeded)
}
