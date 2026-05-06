package qtls

import (
	"errors"
	"io"
	"net"

	"github.com/sagernet/quic-go"
	"github.com/sagernet/quic-go/http3"
)

type quicError struct {
	err error
}

func WrapError(err error) error {
	if err == nil {
		return nil
	}
	return &quicError{err: err}
}

func (e *quicError) Error() string {
	return e.err.Error()
}

func (e *quicError) Unwrap() error {
	return e.err
}

func (e *quicError) Is(target error) bool {
	if errors.Is(e.err, target) {
		return true
	}
	switch target {
	case net.ErrClosed:
		var streamErr *quic.StreamError
		if errors.As(e.err, &streamErr) {
			return !streamErr.Remote && streamErr.ErrorCode == 0
		}
		var transportErr *quic.TransportError
		if errors.As(e.err, &transportErr) {
			return transportErr.ErrorCode == quic.NoError
		}
		var appErr *quic.ApplicationError
		if errors.As(e.err, &appErr) {
			return appErr.Remote && appErr.ErrorCode == 0
		}
		var h3Err *http3.Error
		if errors.As(e.err, &h3Err) {
			return h3Err.ErrorCode == http3.ErrCodeNoError || h3Err.ErrorCode == http3.ErrCodeRequestCanceled
		}
	case io.EOF:
		var streamErr *quic.StreamError
		if errors.As(e.err, &streamErr) {
			return !streamErr.Remote && streamErr.ErrorCode == 0
		}
	}
	return false
}
