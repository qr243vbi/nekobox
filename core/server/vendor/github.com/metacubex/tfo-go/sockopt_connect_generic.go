//go:build freebsd || windows

package tfo

func setTFODialer(fd uintptr) error {
	return setTFO(int(fd), 1)
}
