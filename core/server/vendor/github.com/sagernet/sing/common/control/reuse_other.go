//go:build !darwin && !dragonfly && !freebsd && !linux && !netbsd && !openbsd && !solaris && !windows

package control

func ReuseAddr() Func {
	return nil
}
