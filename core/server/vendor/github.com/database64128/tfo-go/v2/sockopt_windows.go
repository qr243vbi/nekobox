package tfo

import "golang.org/x/sys/windows"

func setTFO(fd, value int) error {
	return windows.SetsockoptInt(windows.Handle(fd), windows.IPPROTO_TCP, windows.TCP_FASTOPEN, value)
}
