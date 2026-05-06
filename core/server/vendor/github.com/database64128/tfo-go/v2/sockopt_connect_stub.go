//go:build !darwin && !freebsd && !linux && !(windows && tfogo_checklinkname0)

package tfo

func setTFODialer(_ uintptr) error {
	return ErrPlatformUnsupported
}
