//go:build !go1.12
// +build !go1.12

package cap

import "os"

// fd returns the file descriptor of the file. Suboptimal because it
// locks a thread to the system calls.
func fd(file *os.File) uintptr {
	return uintptr(file.Fd())
}
