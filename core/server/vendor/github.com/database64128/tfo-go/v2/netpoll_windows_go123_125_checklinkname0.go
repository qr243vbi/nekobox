//go:build windows && !go1.26 && tfogo_checklinkname0

package tfo

import (
	_ "unsafe"

	"golang.org/x/sys/windows"
)

// operation contains superset of data necessary to perform all async IO.
//
// Copied from src/internal/poll/fd_windows.go
type operation struct {
	// Used by IOCP interface, it must be first field
	// of the struct, as our code relies on it.
	o windows.Overlapped

	// fields used by runtime.netpoll
	runtimeCtx uintptr
	mode       int32

	// fields used only by net package
	fd     *pFD
	buf    windows.WSABuf
	msg    windows.WSAMsg
	sa     windows.Sockaddr
	rsa    *windows.RawSockaddrAny
	rsan   int32
	handle windows.Handle
	flags  uint32
	qty    uint32
	bufs   []windows.WSABuf
}

//go:linkname execIO internal/poll.execIO
func execIO(o *operation, submit func(o *operation) error) (int, error)

func (fd *pFD) ConnectEx(ra windows.Sockaddr, b []byte) (n int, err error) {
	n, err = execIO(&fd.wop, func(o *operation) error {
		return windows.ConnectEx(o.fd.Sysfd, ra, &b[0], uint32(len(b)), &o.qty, &o.o)
	})
	return
}
