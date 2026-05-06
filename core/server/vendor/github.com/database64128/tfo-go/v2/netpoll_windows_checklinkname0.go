//go:build windows && tfogo_checklinkname0

package tfo

import (
	"net"
	"time"
	_ "unsafe"

	"golang.org/x/sys/windows"
)

// Network file descriptor.
//
// Copied from src/net/fd_posix.go
type netFD struct {
	pfd pFD

	// immutable until Close
	family      int
	sotype      int
	isConnected bool // handshake completed or use of association with peer
	net         string
	laddr       net.Addr
	raddr       net.Addr
}

//go:linkname newFD net.newFD
func newFD(sysfd windows.Handle, family, sotype int, net string) (*netFD, error)

//go:linkname netFDInit net.(*netFD).init
func netFDInit(fd *netFD) error

//go:linkname netFDClose net.(*netFD).Close
func netFDClose(fd *netFD) error

//go:linkname netFDCtrlNetwork net.(*netFD).ctrlNetwork
func netFDCtrlNetwork(fd *netFD) string

//go:linkname netFDWrite net.(*netFD).Write
func netFDWrite(fd *netFD, p []byte) (int, error)

//go:linkname netFDSetWriteDeadline net.(*netFD).SetWriteDeadline
func netFDSetWriteDeadline(fd *netFD, t time.Time) error

func (fd *netFD) init() error {
	return netFDInit(fd)
}

func (fd *netFD) Close() error {
	return netFDClose(fd)
}

func (fd *netFD) ctrlNetwork() string {
	return netFDCtrlNetwork(fd)
}

func (fd *netFD) Write(p []byte) (int, error) {
	return netFDWrite(fd, p)
}

func (fd *netFD) SetWriteDeadline(t time.Time) error {
	return netFDSetWriteDeadline(fd, t)
}

// Copied from src/net/rawconn.go
type rawConn struct {
	fd *netFD
}

func newRawConn(fd *netFD) *rawConn {
	return &rawConn{fd: fd}
}

//go:linkname rawConnControl net.(*rawConn).Control
func rawConnControl(c *rawConn, f func(uintptr)) error

//go:linkname rawConnRead net.(*rawConn).Read
func rawConnRead(c *rawConn, f func(uintptr) bool) error

//go:linkname rawConnWrite net.(*rawConn).Write
func rawConnWrite(c *rawConn, f func(uintptr) bool) error

func (c *rawConn) Control(f func(uintptr)) error {
	return rawConnControl(c, f)
}

func (c *rawConn) Read(f func(uintptr) bool) error {
	return rawConnRead(c, f)
}

func (c *rawConn) Write(f func(uintptr) bool) error {
	return rawConnWrite(c, f)
}
