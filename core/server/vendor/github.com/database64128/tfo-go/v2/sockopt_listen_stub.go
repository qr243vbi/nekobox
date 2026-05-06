//go:build !darwin && !freebsd && !linux && !windows

package tfo

func setTFOListener(_ uintptr) error {
	return ErrPlatformUnsupported
}

func setTFOListenerWithBacklog(_ uintptr, _ int) error {
	return ErrPlatformUnsupported
}
