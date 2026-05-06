package netx

import "golang.org/x/sys/windows"

// Copied and modified from src/net/ipsock_posix.go
//
// Not sure if all of these are needed on Windows. Windows does not seem to allow
// you to disable protocols or address families.

func probe() (p IPStackCapabilities) {
	s, err := windows.WSASocket(windows.AF_INET, windows.SOCK_STREAM, windows.IPPROTO_TCP, nil, 0, windows.WSA_FLAG_OVERLAPPED|windows.WSA_FLAG_NO_HANDLE_INHERIT)
	switch err {
	case windows.WSAEAFNOSUPPORT, windows.WSAEPROTONOSUPPORT:
	case nil:
		_ = windows.Closesocket(s)
		p.IPv4Enabled = true
	}
	var probes = []struct {
		laddr windows.SockaddrInet6
		value int
	}{
		// IPv6 communication capability
		{laddr: windows.SockaddrInet6{Addr: [16]byte{15: 1}}, value: 1},
		// IPv4-mapped IPv6 address communication capability
		{laddr: windows.SockaddrInet6{Addr: [16]byte{10: 0xff, 11: 0xff, 127, 0, 0, 1}}, value: 0},
	}
	for i := range probes {
		s, err := windows.WSASocket(windows.AF_INET6, windows.SOCK_STREAM, windows.IPPROTO_TCP, nil, 0, windows.WSA_FLAG_OVERLAPPED|windows.WSA_FLAG_NO_HANDLE_INHERIT)
		if err != nil {
			continue
		}
		defer windows.Closesocket(s)
		_ = windows.SetsockoptInt(s, windows.IPPROTO_IPV6, windows.IPV6_V6ONLY, probes[i].value)
		if err := windows.Bind(s, &probes[i].laddr); err != nil {
			continue
		}
		if i == 0 {
			p.IPv6Enabled = true
		} else {
			p.IPv4MappedIPv6Enabled = true
		}
	}
	return
}
