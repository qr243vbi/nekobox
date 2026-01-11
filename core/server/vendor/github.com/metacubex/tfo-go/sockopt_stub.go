//go:build !darwin && !freebsd && !linux && !windows

package tfo

func setTFOListener(fd uintptr) error {
	return ErrPlatformUnsupported
}

func setTFOListenerWithBacklog(fd uintptr, backlog int) error {
	return ErrPlatformUnsupported
}

func setTFODialer(fd uintptr) error {
	return ErrPlatformUnsupported
}
