//go:build darwin || freebsd || linux

package tfo

import (
	"context"
	"errors"
	"net"
	"os"
	"syscall"

	"golang.org/x/sys/unix"
)

func setIPv6Only(fd int, family int, ipv6only bool) error {
	if family == unix.AF_INET6 {
		// Allow both IP versions even if the OS default
		// is otherwise. Note that some operating systems
		// never admit this option.
		return unix.SetsockoptInt(fd, unix.IPPROTO_IPV6, unix.IPV6_V6ONLY, boolint(ipv6only))
	}
	return nil
}

func setNoDelay(fd int, noDelay int) error {
	return unix.SetsockoptInt(fd, unix.IPPROTO_TCP, unix.TCP_NODELAY, noDelay)
}

func ctrlNetwork(network string, family int) string {
	if network == "tcp4" || family == unix.AF_INET {
		return "tcp4"
	}
	return "tcp6"
}

func (d *Dialer) dialSingle(ctx context.Context, network string, laddr, raddr *net.TCPAddr, b []byte, ctrlCtxFn func(context.Context, string, string, syscall.RawConn) error) (*net.TCPConn, error) {
	ltsa := (*tcpSockaddr)(laddr)
	rtsa := (*tcpSockaddr)(raddr)
	family, ipv6only := favoriteAddrFamily(network, ltsa, rtsa, "dial")

	lsa, err := ltsa.sockaddr(family)
	if err != nil {
		return nil, err
	}

	rsa, err := rtsa.sockaddr(family)
	if err != nil {
		return nil, err
	}

	fd, err := d.socket(family)
	if err != nil {
		return nil, wrapSyscallError("socket", err)
	}

	if err = d.setIPv6Only(fd, family, ipv6only); err != nil {
		unix.Close(fd)
		return nil, wrapSyscallError("setsockopt(IPV6_V6ONLY)", err)
	}

	if err = setNoDelay(fd, 1); err != nil {
		unix.Close(fd)
		return nil, wrapSyscallError("setsockopt(TCP_NODELAY)", err)
	}

	if err = setTFODialerFromSocket(uintptr(fd)); err != nil {
		if !d.Fallback || !errors.Is(err, ErrUnsupported) {
			unix.Close(fd)
			return nil, wrapSyscallError("setsockopt("+setTFODialerFromSocketSockoptName+")", err)
		}
		runtimeDialTFOSupport.storeNone()
	}

	f := os.NewFile(uintptr(fd), "")
	defer f.Close()

	rawConn, err := f.SyscallConn()
	if err != nil {
		return nil, err
	}

	if ctrlCtxFn != nil {
		if err = ctrlCtxFn(ctx, ctrlNetwork(network, family), raddr.String(), rawConn); err != nil {
			return nil, err
		}
	}

	if laddr != nil {
		if cErr := rawConn.Control(func(fd uintptr) {
			err = syscall.Bind(int(fd), lsa)
		}); cErr != nil {
			return nil, cErr
		}
		if err != nil {
			return nil, wrapSyscallError("bind", err)
		}
	}

	var (
		n           int
		canFallback bool
	)

	if err = connWriteFunc(ctx, f, func(f *os.File) (err error) {
		n, canFallback, err = connect(rawConn, rsa, b)
		return err
	}); err != nil {
		if d.Fallback && canFallback {
			runtimeDialTFOSupport.storeNone()
			return d.dialAndWriteTCPConn(ctx, network, raddr.String(), b)
		}
		return nil, err
	}

	c, err := net.FileConn(f)
	if err != nil {
		return nil, err
	}

	if n < len(b) {
		if err = netConnWriteBytes(ctx, c, b[n:]); err != nil {
			c.Close()
			return nil, err
		}
	}

	return c.(*net.TCPConn), err
}

func connect(rawConn syscall.RawConn, rsa syscall.Sockaddr, b []byte) (n int, canFallback bool, err error) {
	var done bool

	if perr := rawConn.Write(func(fd uintptr) bool {
		if done {
			return true
		}

		n, err = doConnect(fd, rsa, b)
		if err == unix.EINPROGRESS {
			done = true
			err = nil
			return false
		}
		return true
	}); perr != nil {
		return 0, false, perr
	}

	if err != nil {
		return 0, doConnectCanFallback(err), wrapSyscallError(connectSyscallName, err)
	}

	if perr := rawConn.Control(func(fd uintptr) {
		err = getSocketError(int(fd), connectSyscallName)
	}); perr != nil {
		return 0, false, perr
	}

	return
}

func getSocketError(fd int, call string) error {
	nerr, err := unix.GetsockoptInt(fd, unix.SOL_SOCKET, unix.SO_ERROR)
	if err != nil {
		return wrapSyscallError("getsockopt", err)
	}
	if nerr != 0 {
		return os.NewSyscallError(call, syscall.Errno(nerr))
	}
	return nil
}
