package tfo

import (
	"context"
	"errors"
	"net"
	"os"
	"syscall"

	"golang.org/x/sys/unix"
)

func (lc *ListenConfig) listenTFO(ctx context.Context, network, address string) (net.Listener, error) {
	// When setting TCP_FASTOPEN_FORCE_ENABLE, the socket must be in the TCPS_CLOSED state.
	// This means setting it before listen().
	//
	// However, setting TCP_FASTOPEN requires being in the TCPS_LISTEN state,
	// which means setting it after listen().

	ctrlFn := lc.Control
	llc := *lc
	llc.Control = func(network, address string, c syscall.RawConn) (err error) {
		if ctrlFn != nil {
			if err = ctrlFn(network, address, c); err != nil {
				return err
			}
		}

		if cerr := c.Control(func(fd uintptr) {
			err = setTFOForceEnable(fd)
		}); cerr != nil {
			return cerr
		}

		if err != nil {
			if !lc.Fallback || !errors.Is(err, errors.ErrUnsupported) {
				return os.NewSyscallError("setsockopt(TCP_FASTOPEN_FORCE_ENABLE)", err)
			}
			runtimeListenNoTFO.Store(true)
		}
		return nil
	}

	ln, err := llc.ListenConfig.Listen(ctx, network, address)
	if err != nil {
		return nil, err
	}

	rawConn, err := ln.(*net.TCPListener).SyscallConn()
	if err != nil {
		ln.Close()
		return nil, err
	}

	if cerr := rawConn.Control(func(fd uintptr) {
		err = setTFOListener(fd)
	}); cerr != nil {
		ln.Close()
		return nil, cerr
	}

	if err != nil {
		ln.Close()
		if !lc.Fallback || !errors.Is(err, errors.ErrUnsupported) {
			return nil, os.NewSyscallError("setsockopt(TCP_FASTOPEN)", err)
		}
		runtimeListenNoTFO.Store(true)
	}

	return ln, nil
}

const AF_MULTIPATH = 39

func (d *Dialer) socket(domain int) (fd int, err error) {
	if d.MultipathTCP() {
		domain = AF_MULTIPATH
	}

	syscall.ForkLock.RLock()
	fd, err = unix.Socket(domain, unix.SOCK_STREAM, unix.IPPROTO_TCP)
	if err != nil {
		syscall.ForkLock.RUnlock()
		return 0, os.NewSyscallError("socket", err)
	}
	unix.CloseOnExec(fd)
	syscall.ForkLock.RUnlock()

	if err = unix.SetNonblock(fd, true); err != nil {
		unix.Close(fd)
		return 0, os.NewSyscallError("setnonblock", err)
	}

	return fd, nil
}

func (d *Dialer) setIPv6Only(fd int, family int, ipv6only bool) error {
	if d.MultipathTCP() {
		return nil
	}
	return setIPv6Only(fd, family, ipv6only)
}

const setTFODialerFromSocketSockoptName = "TCP_FASTOPEN_FORCE_ENABLE"

const connectSyscallName = "connectx"

func doConnect(fd uintptr, rsa unix.Sockaddr, b []byte) (int, error) {
	var (
		flags uint32
		iov   []unix.Iovec
	)
	if len(b) > 0 {
		flags = unix.CONNECT_DATA_IDEMPOTENT
		iov = []unix.Iovec{
			{
				Base: &b[0],
				Len:  uint64(len(b)),
			},
		}
	}
	n, err := unix.Connectx(int(fd), 0, nil, rsa, unix.SAE_ASSOCID_ANY, flags, iov, nil)
	return int(n), err
}
