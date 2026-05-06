//go:build windows && tfogo_checklinkname0

package tfo

import (
	"context"
	"errors"
	"net"
	"os"
	"runtime"
	"syscall"
	"unsafe"

	"github.com/database64128/netx-go"
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
	family, ipv6only := favoriteDialAddrFamily(network, laddr, raddr)

	lsa, err := windowsSockaddrFromTCPAddr(laddr, family)
	if err != nil {
		return nil, err
	}
	if lsa == nil {
		// ConnectEx requires a bound socket.
		switch family {
		case windows.AF_INET:
			lsa = &windows.SockaddrInet4{}
		case windows.AF_INET6:
			lsa = &windows.SockaddrInet6{}
		}
	}

	rsa, err := windowsSockaddrFromTCPAddr(raddr, family)
	if err != nil {
		return nil, err
	}

	handle, err := windows.WSASocket(int32(family), windows.SOCK_STREAM, windows.IPPROTO_TCP, nil, 0, windows.WSA_FLAG_OVERLAPPED|windows.WSA_FLAG_NO_HANDLE_INHERIT)
	if err != nil {
		return nil, os.NewSyscallError("WSASocket", err)
	}

	fd, err := newFD(handle, family, windows.SOCK_STREAM, network)
	if err != nil {
		windows.Closesocket(handle)
		return nil, err
	}

	if err = setIPv6Only(handle, family, ipv6only); err != nil {
		fd.Close()
		return nil, os.NewSyscallError("setsockopt(IPV6_V6ONLY)", err)
	}

	if err = setNoDelay(handle, 1); err != nil {
		fd.Close()
		return nil, os.NewSyscallError("setsockopt(TCP_NODELAY)", err)
	}

	if err = setTFODialer(uintptr(handle)); err != nil {
		if !d.Fallback || !errors.Is(err, errors.ErrUnsupported) {
			fd.Close()
			return nil, os.NewSyscallError("setsockopt(TCP_FASTOPEN)", err)
		}
		runtimeDialTFOSupport.storeNone()
	}

	if ctrlCtxFn != nil {
		if err = ctrlCtxFn(ctx, fd.ctrlNetwork(), raddr.String(), newRawConn(fd)); err != nil {
			fd.Close()
			return nil, err
		}
	}

	if err = windows.Bind(handle, lsa); err != nil {
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
			return wrapSyscallError("connectex", err)
		}

		if err = setUpdateConnectContext(handle); err != nil {
			return os.NewSyscallError("setsockopt(SO_UPDATE_CONNECT_CONTEXT)", err)
		}

		lsa, err = windows.Getsockname(handle)
		if err != nil {
			return wrapSyscallError("getsockname", err)
		}
		fd.laddr = tcpAddrFromWindowsSockaddr(lsa)

		rsa, err = windows.Getpeername(handle)
		if err != nil {
			return wrapSyscallError("getpeername", err)
		}
		fd.raddr = tcpAddrFromWindowsSockaddr(rsa)

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

func windowsSockaddrFromTCPAddr(a *net.TCPAddr, family int) (windows.Sockaddr, error) {
	if a == nil {
		return nil, nil
	}
	ip := a.IP
	switch family {
	case windows.AF_INET:
		if len(ip) == 0 {
			ip = net.IPv4zero
		}
		ip4 := ip.To4()
		if ip4 == nil {
			return nil, &net.AddrError{Err: "non-IPv4 address", Addr: ip.String()}
		}
		return &windows.SockaddrInet4{
			Port: a.Port,
			Addr: [4]byte(ip4),
		}, nil
	case windows.AF_INET6:
		if len(ip) == 0 || ip.Equal(net.IPv4zero) {
			ip = net.IPv6zero
		}
		ip6 := ip.To16()
		if ip6 == nil {
			return nil, &net.AddrError{Err: "non-IPv6 address", Addr: ip.String()}
		}
		return &windows.SockaddrInet6{
			Port:   a.Port,
			ZoneId: uint32(netx.ZoneCache.Index(a.Zone)),
			Addr:   [16]byte(ip6),
		}, nil
	}
	return nil, &net.AddrError{Err: "invalid address family", Addr: ip.String()}
}

func tcpAddrFromWindowsSockaddr(sa windows.Sockaddr) *net.TCPAddr {
	switch sa := sa.(type) {
	case *windows.SockaddrInet4:
		return &net.TCPAddr{IP: sa.Addr[0:], Port: sa.Port}
	case *windows.SockaddrInet6:
		return &net.TCPAddr{IP: sa.Addr[0:], Port: sa.Port, Zone: netx.ZoneCache.Name(int(sa.ZoneId))}
	}
	return nil
}
