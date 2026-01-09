//go:build !windows

package buf

import "golang.org/x/sys/unix"

func (b *Buffer) Iovec(length int) unix.Iovec {
	var iov unix.Iovec
	iov.Base = &b.data[b.start]
	iov.SetLen(length)
	return iov
}
