package buf

import "golang.org/x/sys/windows"

func (b *Buffer) Iovec(length int) windows.WSABuf {
	return windows.WSABuf{
		Buf: &b.data[b.start],
		Len: uint32(length),
	}
}
