package exceptions

import (
	"errors"
	"net"
)

type TimeoutError interface {
	Timeout() bool
}

func IsTimeout(err error) bool {
	var netErr net.Error
	if errors.As(err, &netErr) {
		//nolint:staticcheck
		return netErr.Temporary() && netErr.Timeout()
	}
	if timeoutErr, isTimeout := Cast[TimeoutError](err); isTimeout {
		return timeoutErr.Timeout()
	}
	return false
}
