//go:build darwin || freebsd || linux

package tfo

import (
	"context"
	"errors"
	"net"
	"os"
	"syscall"

	"github.com/database64128/netx-go"
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
	family, ipv6only := favoriteDialAddrFamily(network, laddr, raddr)

	fd, err := d.socket(family)
	if err != nil {
		return nil, wrapSyscallError("socket", err)
	}

	if err = d.setIPv6Only(fd, family, ipv6only); err != nil {
		unix.Close(fd)
		return nil, os.NewSyscallError("setsockopt(IPV6_V6ONLY)", err)
	}

	if err = setNoDelay(fd, 1); err != nil {
		unix.Close(fd)
		return nil, os.NewSyscallError("setsockopt(TCP_NODELAY)", err)
	}

	if err = setTFODialerFromSocket(uintptr(fd)); err != nil {
		if !d.Fallback || !errors.Is(err, errors.ErrUnsupported) {
			unix.Close(fd)
			return nil, os.NewSyscallError("setsockopt("+setTFODialerFromSocketSockoptName+")", err)
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
		lsa, err := unixSockaddrFromTCPAddr(laddr, family)
		if err != nil {
			return nil, err
		}

		if cErr := rawConn.Control(func(fd uintptr) {
			err = unix.Bind(int(fd), lsa)
		}); cErr != nil {
			return nil, cErr
		}
		if err != nil {
			return nil, wrapSyscallError("bind", err)
		}
	}

	rsa, err := unixSockaddrFromTCPAddr(raddr, family)
	if err != nil {
		return nil, err
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

func unixSockaddrFromTCPAddr(a *net.TCPAddr, family int) (unix.Sockaddr, error) {
	if a == nil {
		return nil, nil
	}
	ip := a.IP
	switch family {
	case unix.AF_INET:
		if len(ip) == 0 {
			ip = net.IPv4zero
		}
		ip4 := ip.To4()
		if ip4 == nil {
			return nil, &net.AddrError{Err: "non-IPv4 address", Addr: ip.String()}
		}
		return &unix.SockaddrInet4{
			Port: a.Port,
			Addr: [4]byte(ip4),
		}, nil
	case unix.AF_INET6:
		if len(ip) == 0 || ip.Equal(net.IPv4zero) {
			ip = net.IPv6zero
		}
		ip6 := ip.To16()
		if ip6 == nil {
			return nil, &net.AddrError{Err: "non-IPv6 address", Addr: ip.String()}
		}
		return &unix.SockaddrInet6{
			Port:   a.Port,
			ZoneId: uint32(netx.ZoneCache.Index(a.Zone)),
			Addr:   [16]byte(ip6),
		}, nil
	}
	return nil, &net.AddrError{Err: "invalid address family", Addr: ip.String()}
}

func connect(rawConn syscall.RawConn, rsa unix.Sockaddr, b []byte) (n int, canFallback bool, err error) {
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
		return os.NewSyscallError("getsockopt", err)
	}
	if nerr != 0 {
		return os.NewSyscallError(call, syscall.Errno(nerr))
	}
	return nil
}
