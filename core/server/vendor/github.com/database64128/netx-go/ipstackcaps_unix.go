//go:build unix

package netx

import (
	"runtime"

	"golang.org/x/sys/unix"
)

// Copied and modified from src/net/ipsock_posix.go

func probe() (p IPStackCapabilities) {
	s, err := sysSocket(unix.AF_INET, unix.SOCK_STREAM, unix.IPPROTO_TCP)
	switch err {
	case unix.EAFNOSUPPORT, unix.EPROTONOSUPPORT:
	case nil:
		_ = unix.Close(s)
		p.IPv4Enabled = true
	}
	var probes = []struct {
		laddr unix.SockaddrInet6
		value int
	}{
		// IPv6 communication capability
		{laddr: unix.SockaddrInet6{Addr: [16]byte{15: 1}}, value: 1},
		// IPv4-mapped IPv6 address communication capability
		{laddr: unix.SockaddrInet6{Addr: [16]byte{10: 0xff, 11: 0xff, 127, 0, 0, 1}}, value: 0},
	}
	switch runtime.GOOS {
	case "dragonfly", "openbsd":
		// The latest DragonFly BSD and OpenBSD kernels don't
		// support IPV6_V6ONLY=0. They always return an error
		// and we don't need to probe the capability.
		probes = probes[:1]
	}
	for i := range probes {
		s, err := sysSocket(unix.AF_INET6, unix.SOCK_STREAM, unix.IPPROTO_TCP)
		if err != nil {
			continue
		}
		defer unix.Close(s)
		_ = unix.SetsockoptInt(s, unix.IPPROTO_IPV6, unix.IPV6_V6ONLY, probes[i].value)
		if err := unix.Bind(s, &probes[i].laddr); err != nil {
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
