package tfo

import (
	"context"
	"errors"
	"net"
	"os"
	"runtime"
	"syscall"
	"unsafe"

	"golang.org/x/sys/windows"
)

func setIPv6Only(fd windows.Handle, family int, ipv6only bool) error {
	if family == windows.AF_INET6 {
		// Allow both IP versions even if the OS default
		// is otherwise. Note that some operating systems
		// never admit this option.
		return windows.SetsockoptInt(fd, windows.IPPROTO_IPV6, windows.IPV6_V6ONLY, boolint(ipv6only))
	}
	return nil
}

func setNoDelay(fd windows.Handle, noDelay int) error {
	return windows.SetsockoptInt(fd, windows.IPPROTO_TCP, windows.TCP_NODELAY, noDelay)
}

func setUpdateConnectContext(fd windows.Handle) error {
	return windows.Setsockopt(fd, windows.SOL_SOCKET, windows.SO_UPDATE_CONNECT_CONTEXT, nil, 0)
}

func (d *Dialer) dialSingle(ctx context.Context, network string, laddr, raddr *net.TCPAddr, b []byte, ctrlCtxFn func(context.Context, string, string, syscall.RawConn) error) (*net.TCPConn, error) {
	ltsa := (*tcpSockaddr)(laddr)
	rtsa := (*tcpSockaddr)(raddr)
	family, ipv6only := favoriteAddrFamily(network, ltsa, rtsa, "dial")

	var (
		ip   net.IP
		port int
		zone string
	)

	if laddr != nil {
		ip = laddr.IP
		port = laddr.Port
		zone = laddr.Zone
	}

	lsa, err := ipToSockaddr(family, ip, port, zone)
	if err != nil {
		return nil, err
	}

	rsa, err := rtsa.sockaddr(family)
	if err != nil {
		return nil, err
	}

	handle, err := windows.WSASocket(int32(family), windows.SOCK_STREAM, windows.IPPROTO_TCP, nil, 0, windows.WSA_FLAG_OVERLAPPED|windows.WSA_FLAG_NO_HANDLE_INHERIT)
	if err != nil {
		return nil, os.NewSyscallError("WSASocket", err)
	}

	fd, err := newFD(syscall.Handle(handle), family, windows.SOCK_STREAM, network)
	if err != nil {
		windows.Closesocket(handle)
		return nil, err
	}

	if err = setIPv6Only(handle, family, ipv6only); err != nil {
		fd.Close()
		return nil, wrapSyscallError("setsockopt(IPV6_V6ONLY)", err)
	}

	if err = setNoDelay(handle, 1); err != nil {
		fd.Close()
		return nil, wrapSyscallError("setsockopt(TCP_NODELAY)", err)
	}

	if err = setTFODialer(uintptr(handle)); err != nil {
		if !d.Fallback || !errors.Is(err, ErrUnsupported) {
			fd.Close()
			return nil, wrapSyscallError("setsockopt(TCP_FASTOPEN)", err)
		}
		runtimeDialTFOSupport.storeNone()
	}

	if ctrlCtxFn != nil {
		rawConn, _ := fd.SyscallConn()
		if err = ctrlCtxFn(ctx, fd.ctrlNetwork(), raddr.String(), rawConn); err != nil {
			fd.Close()
			return nil, err
		}
	}

	if err = syscall.Bind(syscall.Handle(handle), lsa); err != nil {
		fd.Close()
		return nil, wrapSyscallError("bind", err)
	}

	if err = fd.init(); err != nil {
		fd.Close()
		return nil, err
	}

	if err = connWriteFunc(ctx, fd, func(fd *netFD) error {
		n, err := fd.pfd.ConnectEx(rsa, b)
		if err != nil {
			return os.NewSyscallError("connectex", err)
		}

		if err = setUpdateConnectContext(handle); err != nil {
			return wrapSyscallError("setsockopt(SO_UPDATE_CONNECT_CONTEXT)", err)
		}

		lsa, err = syscall.Getsockname(syscall.Handle(handle))
		if err != nil {
			return wrapSyscallError("getsockname", err)
		}
		fd.laddr = sockaddrToTCP(lsa)

		rsa, err = syscall.Getpeername(syscall.Handle(handle))
		if err != nil {
			return wrapSyscallError("getpeername", err)
		}
		fd.raddr = sockaddrToTCP(rsa)

		if n < len(b) {
			if _, err = fd.Write(b[n:]); err != nil {
				return err
			}
		}

		return nil
	}); err != nil {
		fd.Close()
		return nil, err
	}

	runtime.SetFinalizer(fd, netFDClose)
	return (*net.TCPConn)(unsafe.Pointer(&fd)), nil
}
