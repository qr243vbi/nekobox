package tfo

import (
	"context"
	"errors"
	"net"
	"os"
	"syscall"

	"golang.org/x/sys/unix"
)

const setTFODialerFromSocketSockoptName = "unreachable"

func setTFODialerFromSocket(_ uintptr) error {
	return nil
}

const sendtoImplicitConnectFlag = unix.MSG_FASTOPEN

// doConnectCanFallback returns whether err from [doConnect] indicates lack of TFO support.
func doConnectCanFallback(err error) bool {
	// On Linux, calling sendto() on an unconnected TCP socket with zero or invalid flags
	// returns -EPIPE. This indicates that the MSG_FASTOPEN flag is not recognized by the kernel.
	//
	// -EOPNOTSUPP is returned if the kernel recognizes the flag, but TFO is disabled via sysctl.
	return err == unix.EPIPE || err == unix.EOPNOTSUPP
}

func (a *atomicDialTFOSupport) casLinuxSendto() bool {
	return a.v.CompareAndSwap(uint32(dialTFOSupportDefault), uint32(dialTFOSupportLinuxSendto))
}

func (d *Dialer) dialTFO(ctx context.Context, network, address string, b []byte) (*net.TCPConn, error) {
	if d.Fallback {
		switch runtimeDialTFOSupport.load() {
		case dialTFOSupportNone:
			return d.dialAndWriteTCPConn(ctx, network, address, b)
		case dialTFOSupportLinuxSendto:
			return d.dialTFOFromSocket(ctx, network, address, b)
		}
	}

	var canFallback bool
	ctrlCtxFn := d.ControlContext
	ctrlFn := d.Control
	ld := *d
	ld.ControlContext = func(ctx context.Context, network, address string, c syscall.RawConn) (err error) {
		switch {
		case ctrlCtxFn != nil:
			if err = ctrlCtxFn(ctx, network, address, c); err != nil {
				return err
			}
		case ctrlFn != nil:
			if err = ctrlFn(network, address, c); err != nil {
				return err
			}
		}

		if cerr := c.Control(func(fd uintptr) {
			err = setTFODialer(fd)
		}); cerr != nil {
			return cerr
		}

		if err != nil {
			if d.Fallback && errors.Is(err, errors.ErrUnsupported) {
				canFallback = true
			}
			return os.NewSyscallError("setsockopt(TCP_FASTOPEN_CONNECT)", err)
		}
		return nil
	}

	nc, err := ld.Dialer.DialContext(ctx, network, address)
	if err != nil {
		if d.Fallback && canFallback {
			runtimeDialTFOSupport.casLinuxSendto()
			return d.dialTFOFromSocket(ctx, network, address, b)
		}
		return nil, err
	}
	if err = netConnWriteBytes(ctx, nc, b); err != nil {
		nc.Close()
		return nil, err
	}
	return nc.(*net.TCPConn), nil
}

func dialTCPAddr(network string, laddr, raddr *net.TCPAddr, b []byte) (*net.TCPConn, error) {
	d := Dialer{Dialer: net.Dialer{LocalAddr: laddr}}
	return d.dialTFO(context.Background(), network, raddr.String(), b)
}
