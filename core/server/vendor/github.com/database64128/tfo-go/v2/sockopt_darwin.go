package tfo

import "golang.org/x/sys/unix"

const TCP_FASTOPEN_FORCE_ENABLE = 0x218

// setTFOForceEnable disables the Darwin kernel's brutal TFO backoff mechanism.
func setTFOForceEnable(fd uintptr) error {
	return unix.SetsockoptInt(int(fd), unix.IPPROTO_TCP, TCP_FASTOPEN_FORCE_ENABLE, 1)
}

func setTFODialer(fd uintptr) error {
	return setTFOForceEnable(fd)
}
