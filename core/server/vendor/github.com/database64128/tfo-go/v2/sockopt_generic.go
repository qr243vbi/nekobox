//go:build darwin || freebsd || linux

package tfo

import "golang.org/x/sys/unix"

func setTFO(fd, value int) error {
	return unix.SetsockoptInt(fd, unix.IPPROTO_TCP, unix.TCP_FASTOPEN, value)
}
