//go:build go1.12
// +build go1.12

package cap

import "os"

// fd returns the file descriptor of the file. This is a little more
// elaborate than using file.Fd() as it works around Go's default of
// locking system calls to threads when functions get raw access to
// their file descriptor. We judge that behavior to not be needed in
// our limited use case and don't want to burden applications with the
// corresponding resource overhead of that thread pinning.
//
// See https://sites.google.com/site/fullycapable/getting-started-with-go/some-go-gotchas-with-psx-and-cap-packages
func fd(file *os.File) uintptr {
	s, err := file.SyscallConn()
	if err != nil {
		return ^uintptr(0)
	}
	var exported uintptr
	s.Control(func(fd uintptr) {
		exported = fd
	})
	return exported
}
