package cronet

import (
	"context"
	"errors"
	"net"
	"os"
	"strconv"
	"syscall"
)

//go:generate go run ./cmd/build-naive generate-net-errors

// NetError represents a Chromium network error code.
// Error codes are negative integers defined in Chromium's net/base/net_error_list.h.
type NetError int

// Error implements the error interface with a Go-style lowercase message.
func (e NetError) Error() string {
	if info, ok := netErrorInfo[e]; ok {
		return info.message
	}
	return "network error " + strconv.Itoa(int(e))
}

// Name returns the Chromium error name (e.g., "ERR_CONNECTION_REFUSED").
func (e NetError) Name() string {
	if info, ok := netErrorInfo[e]; ok {
		return info.name
	}
	return "ERR_UNKNOWN_" + strconv.Itoa(int(-e))
}

// Description returns the full description from Chromium source.
func (e NetError) Description() string {
	if info, ok := netErrorInfo[e]; ok {
		return info.description
	}
	return ""
}

// Code returns the raw error code as an integer.
func (e NetError) Code() int {
	return int(e)
}

// Is implements errors.Is() support for comparing NetError with Go standard errors.
func (e NetError) Is(target error) bool {
	if t, ok := target.(NetError); ok {
		return e == t
	}

	switch target {
	case net.ErrClosed:
		return e == NetErrorConnectionClosed ||
			e == NetErrorSocketNotConnected ||
			e == NetErrorConnectionReset ||
			e == NetErrorConnectionAborted

	case os.ErrDeadlineExceeded:
		return e == NetErrorTimedOut ||
			e == NetErrorConnectionTimedOut

	case syscall.ECONNREFUSED:
		return e == NetErrorConnectionRefused

	case syscall.ECONNRESET:
		return e == NetErrorConnectionReset

	case syscall.ECONNABORTED:
		return e == NetErrorConnectionAborted

	case syscall.ETIMEDOUT:
		return e == NetErrorConnectionTimedOut

	case syscall.ENETUNREACH, syscall.EHOSTUNREACH:
		return e == NetErrorAddressUnreachable

	case syscall.ENOTCONN:
		return e == NetErrorSocketNotConnected
	}

	return false
}

// Timeout returns true if this error represents a timeout.
// This implements the net.Error interface.
func (e NetError) Timeout() bool {
	return e == NetErrorTimedOut || e == NetErrorConnectionTimedOut
}

// Temporary returns false. Chromium errors are not considered temporary.
// This implements the net.Error interface.
func (e NetError) Temporary() bool {
	return false
}

func toNetError(err error) NetError {
	if err == nil {
		return 0
	}

	if errors.Is(err, context.Canceled) {
		return NetErrorAborted
	}
	if errors.Is(err, context.DeadlineExceeded) {
		return NetErrorConnectionTimedOut
	}

	var networkError net.Error
	if errors.As(err, &networkError) && networkError.Timeout() {
		return NetErrorConnectionTimedOut
	}

	var syscallError *os.SyscallError
	if errors.As(err, &syscallError) {
		if errno, ok := syscallError.Err.(syscall.Errno); ok {
			switch errno {
			case syscall.ECONNREFUSED:
				return NetErrorConnectionRefused
			case syscall.ETIMEDOUT:
				return NetErrorConnectionTimedOut
			case syscall.ENETUNREACH, syscall.EHOSTUNREACH:
				return NetErrorAddressUnreachable
			case syscall.ECONNRESET:
				return NetErrorConnectionReset
			case syscall.ECONNABORTED:
				return NetErrorConnectionAborted
			}
		}
	}

	return NetErrorConnectionFailed
}
