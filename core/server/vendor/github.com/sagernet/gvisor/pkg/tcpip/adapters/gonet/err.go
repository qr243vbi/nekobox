package gonet

import (
	"fmt"
	"syscall"

	"github.com/sagernet/gvisor/pkg/tcpip"
)

func TranslateNetstackError(err tcpip.Error) error {
	switch err.(type) {
	case nil:
		return nil
	case *tcpip.ErrUnknownProtocol:
		return syscall.EINVAL
	case *tcpip.ErrUnknownNICID:
		return syscall.ENODEV
	case *tcpip.ErrUnknownDevice:
		return syscall.ENODEV
	case *tcpip.ErrUnknownProtocolOption:
		return syscall.ENOPROTOOPT
	case *tcpip.ErrDuplicateNICID:
		return syscall.EEXIST
	case *tcpip.ErrDuplicateAddress:
		return syscall.EEXIST
	case *tcpip.ErrHostUnreachable:
		return syscall.EHOSTUNREACH
	case *tcpip.ErrHostDown:
		return syscall.EHOSTDOWN
	case *tcpip.ErrNoNet:
		return errNoNet
	case *tcpip.ErrAlreadyBound:
		return syscall.EINVAL
	case *tcpip.ErrInvalidEndpointState:
		return syscall.EINVAL
	case *tcpip.ErrAlreadyConnecting:
		return syscall.EALREADY
	case *tcpip.ErrAlreadyConnected:
		return syscall.EISCONN
	case *tcpip.ErrNoPortAvailable:
		return syscall.EAGAIN
	case *tcpip.ErrPortInUse:
		return syscall.EADDRINUSE
	case *tcpip.ErrBadLocalAddress:
		return syscall.EADDRNOTAVAIL
	case *tcpip.ErrClosedForSend:
		return syscall.EPIPE
	case *tcpip.ErrClosedForReceive:
		return syscall.ENOTCONN
	case *tcpip.ErrWouldBlock:
		return syscall.EWOULDBLOCK
	case *tcpip.ErrConnectionRefused:
		return syscall.ECONNREFUSED
	case *tcpip.ErrTimeout:
		return syscall.ETIMEDOUT
	case *tcpip.ErrAborted:
		return syscall.EPIPE
	case *tcpip.ErrConnectStarted:
		return syscall.EINPROGRESS
	case *tcpip.ErrDestinationRequired:
		return syscall.EDESTADDRREQ
	case *tcpip.ErrNotSupported:
		return syscall.EOPNOTSUPP
	case *tcpip.ErrQueueSizeNotSupported:
		return syscall.ENOTTY
	case *tcpip.ErrNotConnected:
		return syscall.ENOTCONN
	case *tcpip.ErrConnectionReset:
		return syscall.ECONNRESET
	case *tcpip.ErrConnectionAborted:
		return syscall.ECONNABORTED
	case *tcpip.ErrNoSuchFile:
		return syscall.ENOENT
	case *tcpip.ErrInvalidOptionValue:
		return syscall.EINVAL
	case *tcpip.ErrBadAddress:
		return syscall.EFAULT
	case *tcpip.ErrNetworkUnreachable:
		return syscall.ENETUNREACH
	case *tcpip.ErrMessageTooLong:
		return syscall.EMSGSIZE
	case *tcpip.ErrNoBufferSpace:
		return syscall.ENOBUFS
	case *tcpip.ErrBroadcastDisabled:
		return syscall.EACCES
	case *tcpip.ErrNotPermitted:
		return syscall.EPERM
	case *tcpip.ErrAddressFamilyNotSupported:
		return syscall.EAFNOSUPPORT
	case *tcpip.ErrBadBuffer:
		return syscall.EFAULT
	case *tcpip.ErrMalformedHeader:
		return syscall.EINVAL
	case *tcpip.ErrInvalidPortRange:
		return syscall.EINVAL
	case *tcpip.ErrMulticastInputCannotBeOutput:
		return syscall.EINVAL
	case *tcpip.ErrMissingRequiredFields:
		return syscall.EINVAL
	default:
		panic(fmt.Sprintf("unknown error %T", err))
	}
}
