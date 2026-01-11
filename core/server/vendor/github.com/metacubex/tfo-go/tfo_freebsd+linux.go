//go:build freebsd || linux

package tfo

import (
	"syscall"

	"golang.org/x/sys/unix"
)

func (*Dialer) socket(domain int) (int, error) {
	return unix.Socket(domain, unix.SOCK_STREAM|unix.SOCK_NONBLOCK|unix.SOCK_CLOEXEC, unix.IPPROTO_TCP)
}

func (*Dialer) setIPv6Only(fd int, family int, ipv6only bool) error {
	return setIPv6Only(fd, family, ipv6only)
}

const connectSyscallName = "sendmsg"

func doConnect(fd uintptr, rsa syscall.Sockaddr, b []byte) (int, error) {
	return syscall.SendmsgN(int(fd), b, nil, rsa, sendtoImplicitConnectFlag|unix.MSG_NOSIGNAL)
}
