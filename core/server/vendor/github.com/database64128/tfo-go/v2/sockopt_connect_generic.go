//go:build freebsd || (windows && tfogo_checklinkname0)

package tfo

func setTFODialer(fd uintptr) error {
	return setTFO(int(fd), 1)
}
