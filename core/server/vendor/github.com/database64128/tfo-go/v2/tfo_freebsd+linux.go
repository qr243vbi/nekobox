//go:build freebsd || linux

package tfo

import (
	"golang.org/x/sys/unix"
)

func (*Dialer) socket(domain int) (int, error) {
	return unix.Socket(domain, unix.SOCK_STREAM|unix.SOCK_NONBLOCK|unix.SOCK_CLOEXEC, unix.IPPROTO_TCP)
}

func (*Dialer) setIPv6Only(fd int, family int, ipv6only bool) error {
	return setIPv6Only(fd, family, ipv6only)
}

const connectSyscallName = "sendmsg"

func doConnect(fd uintptr, rsa unix.Sockaddr, b []byte) (int, error) {
	return unix.SendmsgN(int(fd), b, nil, rsa, sendtoImplicitConnectFlag|unix.MSG_NOSIGNAL)
}
