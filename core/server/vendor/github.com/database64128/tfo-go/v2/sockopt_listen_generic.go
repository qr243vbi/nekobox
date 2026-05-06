//go:build darwin || freebsd || windows

package tfo

func setTFOListener(fd uintptr) error {
	return setTFO(int(fd), 1)
}

func setTFOListenerWithBacklog(fd uintptr, _ int) error {
	return setTFOListener(fd)
}
